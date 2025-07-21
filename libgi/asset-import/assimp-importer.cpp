#include "asset-import.h"

#include "libgi/color.h"
#include "libgi/util.h"
#include "libgi/subdivision.h"

#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <filesystem>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>
#include <assimp/material.h>

#include <glm/gtx/matrix_transform_2d.hpp>

using namespace glm;
using namespace std;

namespace import {
	void mesh_load_process_node(aiNode *node_ai, const aiScene *scene_ai, mat4 parent_trafo, mat4 model_trafo, unsigned material_offset, 
								std::vector<std::tuple<int,int,int>> &light_geom, int &light_prims, scene &rtgi_scene, const uint subdiv_level, const bool subd_type_patches);

	inline vec3 to_glm(const aiVector3D& v) { return vec3(v.x, v.y, v.z); }

	// from https://stackoverflow.com/questions/73611341/assimp-gltf-meshes-not-properly-scaled
	mat4 to_glm(const aiMatrix4x4 &from) {
		mat4 to;
		
		to[0][0] = from.a1; to[0][1] = from.b1;  to[0][2] = from.c1; to[0][3] = from.d1;
		to[1][0] = from.a2; to[1][1] = from.b2;  to[1][2] = from.c2; to[1][3] = from.d2;
		to[2][0] = from.a3; to[2][1] = from.b3;  to[2][2] = from.c3; to[2][3] = from.d3;
		to[3][0] = from.a4; to[3][1] = from.b4;  to[3][2] = from.c4; to[3][3] = from.d4;

		return to;
	}

	glm::vec4 to_glm_vec4(const aiVector3D &from) {
		return glm::vec4(from.x, from.y, from.z, 1.0f);
	}

	void assimp_importer::load_scene(const std::filesystem::path& filepath) {
		this->filepath = filepath;

		unsigned int flags;  // | aiProcess_FlipUVs  // TODO assimp
		if (subdiv_level == 0) {
			flags = aiProcess_GenNormals;
			flags |= aiProcess_Triangulate;
		} else {
			flags = aiProcess_JoinIdenticalVertices;
			flags |= aiProcess_DropNormals;
		}

		scene_ai = importer.ReadFile(filepath.string(), flags);
		if (!scene_ai) // handle error
			throw std::runtime_error("ERROR: Failed to load file: " + filepath.string() + "!");

	}

	void assimp_importer::import(scene& scene) {
		// todo: store indices prior to adding anything to allow "transform-last"

		// load materials
		//const aiScene *scene_ai = opt_scene_ai.value();
		if (!scene_ai)
			throw std::runtime_error("ERROR: aiScene has not been initialized!");
		unsigned material_offset = scene.materials.size();
		for (uint32_t i = 0; i < scene_ai->mNumMaterials; ++i) {
			::material material;
			aiString name_ai;
			aiColor3D col;
			auto mat_ai = scene_ai->mMaterials[i];
			mat_ai->Get(AI_MATKEY_NAME, name_ai);
			if (name != "") material.name = name + "/" + name_ai.C_Str();
			else            material.name = name_ai.C_Str();
			
			vec3 kd(0), ks(0), ke(0);
			float tmp;
			if (mat_ai->Get(AI_MATKEY_COLOR_DIFFUSE,  col) == AI_SUCCESS) kd = vec4(col.r, col.g, col.b, 1.0f);
			if (mat_ai->Get(AI_MATKEY_COLOR_SPECULAR, col) == AI_SUCCESS) ks = vec4(col.r, col.g, col.b, 1.0f);
			if (mat_ai->Get(AI_MATKEY_COLOR_EMISSIVE, col) == AI_SUCCESS) ke = vec4(col.r, col.g, col.b, 1.0f);
			if (mat_ai->Get(AI_MATKEY_SHININESS,      tmp) == AI_SUCCESS) material.roughness = roughness_from_exponent(tmp);
			if (mat_ai->Get(AI_MATKEY_REFRACTI,       tmp) == AI_SUCCESS) material.ior = tmp;
			if (material.ior == 1.0f) material.ior = 1.3;
			if (luma(kd) > 1e-4) material.albedo = kd;
			else                 material.albedo = ks;
			material.albedo = pow(material.albedo, vec3(2.2f, 2.2f, 2.2f));
			material.emissive = ke;
			
			if (mat_ai->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
				aiString path_ai;
				mat_ai->GetTexture(aiTextureType_DIFFUSE, 0, &path_ai);
				filesystem::path p = filepath.parent_path() / path_ai.C_Str();

				if (mat_ai->GetTextureCount(aiTextureType_OPACITY) > 0) {
					aiString mask_path_ai;
					mat_ai->GetTexture(aiTextureType_OPACITY, 0, &mask_path_ai);
					filesystem::path mask_path = filepath.parent_path() / mask_path_ai.C_Str();
					material.albedo_tex = load_image4f(p, &mask_path);
				} else {
					material.albedo_tex = load_image4f(p);
				}
				scene.textures.push_back(material.albedo_tex);
			}

	#ifndef RTGI_SKIP_BRDF
			material.brdf = scene.brdfs["default"];
	#endif
		
			scene.materials.push_back(material);
		}

		int light_prims = 0;
		std::vector<std::tuple<int,int,int>> light_geom;

		// load meshes
		mesh_load_process_node(scene_ai->mRootNode, scene_ai, mat4(1.0f), trafo, material_offset, light_geom, light_prims, scene, subdiv_level, subdiv_type_patches);
		
	}

	// from https://stackoverflow.com/questions/73611341/assimp-gltf-meshes-not-properly-scaled
	// Recursive load function for assimp that applies the transformation matrices of the node hierarchy to the loaded data
	void mesh_load_process_node(aiNode *node_ai, const aiScene *scene_ai, mat4 parent_trafo, mat4 model_trafo, unsigned material_offset, 
								std::vector<std::tuple<int,int,int>> &light_geom, int &light_prims, scene &rtgi_scene, const uint subdiv_level, const bool subd_type_patches) {
		mat4 node_trafo = parent_trafo * to_glm(node_ai->mTransformation);
		mat4 transform = model_trafo * node_trafo;
		mat3 normal_transform = transpose(inverse(mat3(transform)));
		for (int i = 0; i < node_ai->mNumMeshes; i++) {
			aiMesh *mesh_ai = scene_ai->mMeshes[node_ai->mMeshes[i]];
			
			// load mesh data
			uint32_t material_id = mesh_ai->mMaterialIndex + material_offset;
			uint32_t index_offset = rtgi_scene.vertices.size();
			auto mat_ai = scene_ai->mMaterials[mesh_ai->mMaterialIndex];
			
			aiString name_ai;
			mat_ai->Get(AI_MATKEY_NAME, name_ai);
			if (any_of(rtgi_scene.mtl_blacklist.begin(), rtgi_scene.mtl_blacklist.end(), [&name_ai](string n) { return n == name_ai.C_Str(); }))
				continue;
			
			if (rtgi_scene.materials[material_id].emissive != vec3(0)) {
				light_geom.push_back({(int)rtgi_scene.triangles.size(), (int)(rtgi_scene.triangles.size()+mesh_ai->mNumFaces), material_id});
				light_prims += mesh_ai->mNumFaces;
			}

			aiUVTransform uvt;
			glm::mat3 uv_trafo(1);
			if (mat_ai->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_BASE_COLOR, 0), uvt) == AI_SUCCESS)
				uv_trafo = glm::translate(glm::rotate(glm::scale(uv_trafo, vec2(uvt.mScaling.x,uvt.mScaling.y)), uvt.mRotation), vec2(uvt.mTranslation.x,uvt.mTranslation.y));
			
			if (subdiv_level == 0) {
				for (uint32_t i = 0; i < mesh_ai->mNumVertices; ++i) {
					vertex vertex;
					vertex.pos = glm::vec3(transform * to_glm_vec4(mesh_ai->mVertices[i]));
					// Normals are transformed like this instead https://stackoverflow.com/questions/59833642/loading-a-collada-dae-model-from-assimp-shows-incorrect-normals
					vertex.norm = normalize(glm::vec3(normal_transform * to_glm_vec4(mesh_ai->mNormals[i])));
					if (mesh_ai->HasTextureCoords(0))
						vertex.tc = glm::vec2(uv_trafo * to_glm(mesh_ai->mTextureCoords[0][i]));
					else
						vertex.tc = vec2(0,0);
					rtgi_scene.vertices.push_back(vertex);
					rtgi_scene.scene_bounds.grow(vertex.pos);
				}
				
				for (uint32_t i = 0; i < mesh_ai->mNumFaces; ++i) {
					const aiFace &face = mesh_ai->mFaces[i];
					if (face.mNumIndices == 3) {
						triangle triangle;
						triangle.a = face.mIndices[0] + index_offset;
						triangle.b = face.mIndices[1] + index_offset;
						triangle.c = face.mIndices[2] + index_offset;
						// test if geom normal agrees with shading normals
						// if not, flip winding order
						auto a = rtgi_scene.vertices[triangle.a];
						auto b = rtgi_scene.vertices[triangle.b];
						auto c = rtgi_scene.vertices[triangle.c];
						if (!same_hemisphere(cross(b.pos-a.pos,c.pos-a.pos), (a.norm+b.norm+c.norm)*0.333f))
							std::swap(triangle.b, triangle.c);
						// append
						triangle.material_id = material_id;
						rtgi_scene.triangles.push_back(triangle);
					}
					else
						std::cout << "WARN: Mesh: skipping non-triangle [" << face.mNumIndices << "] face (that the ass imp did not triangulate)!" << std::endl;
				}
			}
			else {
				// object with control mesh vertices and faces
				// -> load data from Assimp import
				subd::object o(mesh_ai, subd_type_patches, name_ai.C_Str());

				for (auto &vert : o.mesh.vertices) {
					// cut off ctrl_vertex to regular vertex
					vert.pos = glm::vec3(transform * vec4(vert.pos, 1.f));
					// Normals are transformed like this instead https://stackoverflow.com/questions/59833642/loading-a-collada-dae-model-from-assimp-shows-incorrect-normals
					vert.norm = normalize(glm::vec3(normal_transform * vec4(vert.norm, 1.f)));
					if (o.mesh.has_texture)
						vert.tc = glm::vec2(uv_trafo * vec3(vert.tc, 1.f));
					else
						vert.tc = vec2(0,0);

				}
				for (auto &normal : o.mesh.normals)
					normal = normalize(glm::vec3(normal_transform * vec4(normal, 1.f)));

				// Subdivide object
				o.mesh.subdivide(subdiv_level);


				if (subd_type_patches) {

					// Add "dummy triangles" to the scene representing the extent of the patches.
					// These are used to identify and include the second level patch BVHs when
					// building the first level BVH
					auto &patches = o.mesh.patches;
					int patch_offset = rtgi_scene.patches.size();
					for (int p = 0; p < patches.size(); p++) {
						auto &patch = patches[p];
						patch.material_id = material_id;

						const auto &root_box = patch.root_box;
						rtgi_scene.scene_bounds.grow(root_box);

						triangle dummy_tri;
						vertex v;
						v.pos = root_box.min;
						rtgi_scene.vertices.push_back(v);
						dummy_tri.a = rtgi_scene.vertices.size()-1;

						v.pos = root_box.max;
						rtgi_scene.vertices.push_back(v);
						dummy_tri.b = rtgi_scene.vertices.size()-1;

						// Add again, because only extent of the volume is relevant, not the tri itself
						rtgi_scene.vertices.push_back(v);
						dummy_tri.c = rtgi_scene.vertices.size()-1;

						dummy_tri.material_id = ((uint32_t)-1) - (patch_offset + p); // reference to the patch id

						rtgi_scene.triangles.push_back(dummy_tri);
					}

					// Store patches into scene
					rtgi_scene.patches.insert(rtgi_scene.patches.end(), patches.begin(), patches.end());
				}
				else {

					// triangulate quad faces
					o.mesh.triangulate();

					// serialize vertices
					vector<vertex> serialized_verts;
					for (int i = 0; i < o.mesh.faces.size(); i++) {
						subd::face &f = o.mesh.faces[i];
						for (int j = 0; j < f.verts.size(); j++) {
							subd::vertex_config &v_cfg = f.verts[j];
							vertex v;
							v.pos = o.mesh.vertices[v_cfg.pos].pos;
							v.tc = o.mesh.tex_coords[v_cfg.tc];
							v.norm = o.mesh.vertices[v_cfg.pos].norm;
							serialized_verts.push_back(v);
						}
					}

					// store data in scene
					for (uint32_t i = 0; i < serialized_verts.size(); ++i) {
						// cut off ctrl_vertex to regular vertex
						vertex vertex = serialized_verts[i];
						rtgi_scene.vertices.push_back(vertex);
						rtgi_scene.scene_bounds.grow(vertex.pos);
					}

					for (uint32_t i = 0; i < serialized_verts.size(); i+=3) {
						triangle triangle;
						triangle.a = index_offset + i;
						triangle.b = index_offset + i+1;
						triangle.c = index_offset + i+2;
						// test if geom normal agrees with shading normals
						// if not, flip winding order
						auto a = rtgi_scene.vertices[triangle.a];
						auto b = rtgi_scene.vertices[triangle.b];
						auto c = rtgi_scene.vertices[triangle.c];
						if (!same_hemisphere(cross(b.pos-a.pos,c.pos-a.pos), (a.norm+b.norm+c.norm)*0.333f))
							std::swap(triangle.b, triangle.c);
						// append
						triangle.material_id = material_id;
						rtgi_scene.triangles.push_back(triangle);
					}

				}

			}
			
		}
		
		for (int i = 0; i < node_ai->mNumChildren; i++)
			mesh_load_process_node(node_ai->mChildren[i], scene_ai, node_trafo, model_trafo, material_offset, light_geom, light_prims, rtgi_scene, subdiv_level, subd_type_patches);
	}
}