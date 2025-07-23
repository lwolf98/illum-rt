#include "asset-import.h"

#include "libgi/color.h"
#include "libgi/util.h"
#include "libgi/subdivision.h"
#include "libgi/material.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/input.h>

using namespace pxr;
using namespace glm;
using namespace std;

namespace import {
	void usd_importer::load_scene(const std::filesystem::path& filepath) {
		this->filepath = filepath;

		std::cout << "USD importer load called!" << std::endl;

		// Open the stage (USD file)
		stage = UsdStage::Open(filepath.c_str());
		if (!stage) {
			throw std::runtime_error("ERROR: Failed to load file: " + filepath.string() + "!");
		}
	}

	void usd_importer::traverse_shader_inputs(const UsdShadeShader &shader, int level, material &material) {
		std::string space(level, '\t');

		TfToken shaderId;
		shader.GetIdAttr().Get(&shaderId);
		std::cout << space << "Shader id: " << shaderId << std::endl;

		// Iterate over shader inputs
		std::vector<UsdShadeInput> shaderInputs = shader.GetInputs();
		for (const auto& shaderInput : shaderInputs) {
			std::cout << space << "Shader input: " << shaderInput.GetFullName() << std::endl;

			// Check if the input is connected to another shader
			UsdShadeConnectableAPI source;
			TfToken inputName;
			UsdShadeAttributeType type;
			if (shaderInput.GetConnectedSource(&source, &inputName, &type)) {
				UsdPrim sourcePrim = source.GetPrim();
				if (sourcePrim.IsA<UsdShadeShader>()) {
					// If the input is connected to another shader, print the connected shader path
					UsdShadeShader connectedShader(sourcePrim);
					std::cout << space << "Input connected to shader: " << connectedShader.GetPath() << std::endl;
					
					// You can now recursively handle the connected shader's inputs as well
					traverse_shader_inputs(connectedShader, level+1, material);
				}
				else {
					// If the input is not connected to another shader, fetch its value (color, float, etc.)
					VtValue inputValue;
					if (shaderInput.Get(&inputValue)) {
						std::cout << space << "Input value: " << inputValue << std::endl;
					}
				}
			} else {
				// If not connected, print the input value
				VtValue inputValue;
				if (shaderInput.Get(&inputValue)) {
					std::cout << space << "Input value: " << inputValue << std::endl;

					if (shaderInput.GetFullName() == "inputs:file") {
						std::cout << space << "DONE: Assign texture file" << std::endl;
						std::string tex_path = inputValue.Get<SdfAssetPath>().GetAssetPath();
						filesystem::path p = filepath.parent_path() / tex_path.erase(0, 2);
						material.albedo_tex = load_image4f(p);
						std::cout << "Filepath: " << p << std::endl;
					}
					else if (shaderInput.GetFullName() == "inputs:roughness") {
						float roughness = inputValue.Get<float>();
						material.roughness = roughness;
					}
					else if (shaderInput.GetFullName() == "inputs:ior") {
						float ior = inputValue.Get<float>();
						material.ior = ior;
					}
					else if (shaderInput.GetFullName() == "inputs:diffuseColor") {
						GfVec3f color = inputValue.Get<GfVec3f>();
						vec3 kd(color[0], color[1], color[2]);
						if (luma(kd) > 1e-4)
							material.albedo = kd;

					}
					else if (shaderInput.GetFullName() == "inputs:specularColor") {
						GfVec3f color = inputValue.Get<GfVec3f>();
						vec3 ks(color[0], color[1], color[2]);
						if (luma(material.albedo) < 1e-4)
							material.albedo = ks;

					}
					else if (shaderInput.GetFullName() == "inputs:emissiveColor") {
						GfVec3f color = inputValue.Get<GfVec3f>();
						material.emissive = vec3(color[0], color[1], color[2]);
					}

				}
			}
		}
	}

	/*
	* Loads the material referenced by mesh into the scene and
	* returns the material id relative to this USD asset import.
	* If a material is already loaded in the scene, only its
	* id will be returned.
	* In case an error occurs and no material could be loaded, -1 is returned.
	*/
	int usd_importer::load_material(const UsdPrim &prim, scene& scene) {
		int material_id = -1;

		UsdShadeMaterialBindingAPI binding(prim);
		UsdRelationship materialRel = binding.GetDirectBindingRel();
		if (materialRel) {
			std::vector<SdfPath> materialPaths;
			materialRel.GetTargets(&materialPaths);

			if (!materialPaths.empty()) {
				std::cout << "Number of assigned materials: " << materialPaths.size() << std::endl;
				// Assume that one mesh has only ONE material
				// Is it even possible that it would have multiple materials?
				// How would that make sense?
				assert(materialPaths.size() == 1);

				SdfPath material_path = materialPaths[0];
				std::string path_key = material_path.GetString();
				if (material_map.count(path_key) > 0) {
					// only return id of material if already loaded
					return material_map[path_key];
				}

				// Now we have the material, let's load it
				UsdShadeMaterial material(stage->GetPrimAtPath(material_path));  // Resolve the material
				std::cout << "Material assigned to mesh: " << material.GetPath() << std::endl;

				// Accessing the material's surface output (it connects to the shader)
				UsdShadeOutput surfaceOutput = material.GetOutput(TfToken("surface"));
				if (surfaceOutput) {
					std::cout << "Material has a surface output connected to: " << surfaceOutput.GetFullName() << std::endl;

					// Now, traverse the connected shader inputs
					UsdShadeConnectableAPI source;
					TfToken inputName;
					UsdShadeAttributeType type;
					if (!surfaceOutput.GetConnectedSource(&source, &inputName, &type)) {
						std::cout << "No connected source found for surface output." << std::endl;
						return -1;
					}
					UsdShadeShader shader(source.GetPrim());
					if (shader) {
						std::cout << "Shader connected to surface output: " << shader.GetPath() << std::endl;

						::material new_mat;
						new_mat.name = material.GetPath().GetName();
						new_mat.brdf = scene.brdfs["default"];
						traverse_shader_inputs(shader, 1, new_mat);

						// PBR corrections
						if (new_mat.ior == 1.0f) new_mat.ior = 1.3;
						new_mat.albedo = pow(new_mat.albedo, vec3(2.2f, 2.2f, 2.2f));

						scene.materials.push_back(new_mat);

						std::string material_key = material_path.GetString();
						material_map.insert({ material_key, material_map.size() });
						material_id = material_map[material_key];
						std::cout << "Materials size: " << scene.materials.size() << std::endl;
						std::cout << "New Material:\n"
							<< "name: " << new_mat.name << std::endl
							<< "albedo: " << new_mat.albedo << std::endl
							<< "emissive: " << new_mat.emissive << std::endl
							<< "tex: " << new_mat.albedo_tex << std::endl
							<< "ior: " << new_mat.ior << std::endl
							<< "roughness: " << new_mat.roughness << std::endl
							<< "brdf: " << new_mat.brdf << std::endl;
					}
				} else {
					std::cout << "Material surface output is not connected." << std::endl;
				}
			}
		}

		return material_id;
	}

	glm::mat4 get_mesh_trafo(const UsdGeomMesh &mesh) {;
		UsdGeomXformable xformable(UsdGeomXform(mesh.GetPrim().GetParent()));
		if (!xformable) {
			std::cout << "Error! Mesh not formable" << std::endl;
		}

		bool resetsXformStack = false;
		std::vector<UsdGeomXformOp> xformOps = xformable.GetOrderedXformOps(&resetsXformStack);
		GfMatrix4d transform(1.0);

		for (const auto &op : xformOps) {
			VtValue opValue;
			op.Get(&opValue);
			GfMatrix4d opTransform = op.GetOpTransform(op.GetOpType(), opValue);
			transform = opTransform * transform;
		}

		return glm::make_mat4(transform.GetArray());
	}

	glm::mat4 get_orientation_trafo(glm::vec3 scene_up, std::string usd_up_axis) {
		scene_up = normalize(scene_up);
		vec3 usd_up_vector;
		std::string a = usd_up_axis;
		if (a == "x" || a == "X") usd_up_vector = vec3(1,0,0);
		else if (a == "y" || a == "Y") usd_up_vector = vec3(0,1,0);
		else if (a == "z" || a == "Z") usd_up_vector = vec3(0,0,1);
		else usd_up_vector = vec3(0,1,0); // default to Y axis
		float rad = std::acos(dot(scene_up, usd_up_vector));
		vec3 rotation_vector = cross(usd_up_vector, scene_up);
		return glm::rotate(mat4(1), rad, rotation_vector);
	}

	void usd_importer::import(scene& scene) {
		unsigned material_offset = scene.materials.size();
		material_map.clear();

		// Import meshes
		for (auto prim : stage->Traverse()) {
			if (prim.IsA<UsdGeomMesh>()) {
				UsdGeomMesh mesh(prim);
				std::cout << "Found mesh: " << prim.GetPath() << std::endl;

				// Init transformation matrices
				mat4 parent_trafo(1);
				VtValue usd_up;
				bool success = stage->GetMetadata(TfToken("upAxis"), &usd_up);
				const mat4 orientation = get_orientation_trafo(scene.up, usd_up.Get<TfToken>().GetString());
				const mat4 &model_trafo = trafo;
				mat4 node_trafo = parent_trafo * get_mesh_trafo(mesh);
				mat4 transform = model_trafo * orientation * node_trafo;
				mat3 normal_transform = transpose(inverse(mat3(transform)));

				uint32_t index_offset = scene.vertices.size();

				//TODO: Implement this section for USD
				/*auto mat_ai = scene_ai->mMaterials[mesh_ai->mMaterialIndex];
				
				aiString name_ai;
				mat_ai->Get(AI_MATKEY_NAME, name_ai);
				if (any_of(rtgi_scene.mtl_blacklist.begin(), rtgi_scene.mtl_blacklist.end(), [&name_ai](string n) { return n == name_ai.C_Str(); }))
					continue;
				
				if (rtgi_scene.materials[material_id].emissive != vec3(0)) {
					light_geom.push_back({(int)rtgi_scene.triangles.size(), (int)(rtgi_scene.triangles.size()+mesh_ai->mNumFaces), material_id});
					light_prims += mesh_ai->mNumFaces;
				}*/

				glm::mat3 uv_trafo(1);
				//TODO: load transforms!
				/*if (mat_ai->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_BASE_COLOR, 0), uvt) == AI_SUCCESS) {
					uv_trafo = glm::translate(glm::rotate(glm::scale(uv_trafo, vec2(uvt.mScaling.x,uvt.mScaling.y)), uvt.mRotation), vec2(uvt.mTranslation.x,uvt.mTranslation.y));
				}*/

				/* Load */

				// Load geometry as object (preparation for subdivision)
				subd::object o(mesh, subdiv_type_patches);

				// Load materials from mesh's GeomSubsets
				vector<UsdGeomSubset> subsets = UsdGeomSubset::GetAllGeomSubsets(mesh);
				for (const UsdGeomSubset &subset : subsets) {
					int subset_mat_id = load_material(subset.GetPrim(), scene);
					VtIntArray indices;
					subset.GetIndicesAttr().Get(&indices);
					for (int face_id : indices) {
						o.mesh.faces[face_id].material_id = material_offset + subset_mat_id;
					}
				}

				// Load base mesh material
				int material_id = load_material(prim, scene);
				if (material_id == -1)
					material_id = 0;
				material_id += material_offset;
				for (auto &f : o.mesh.faces)
					if (f.material_id == -1)
						f.material_id = material_id;

				// Apply transformations
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

				if (subdiv_level == 0) {
					// Triangulate quad faces
					o.mesh.triangulate();

					// Serialize vertices
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
						scene.vertices.push_back(vertex);
						scene.scene_bounds.grow(vertex.pos);
					}

					for (uint32_t i = 0; i < serialized_verts.size(); i+=3) {
						triangle triangle;
						triangle.material_id = o.mesh.faces[i/3].material_id;

						triangle.a = index_offset + i;
						triangle.b = index_offset + i+1;
						triangle.c = index_offset + i+2;
						// test if geom normal agrees with shading normals
						// if not, flip winding order
						auto a = scene.vertices[triangle.a];
						auto b = scene.vertices[triangle.b];
						auto c = scene.vertices[triangle.c];
						if (!same_hemisphere(cross(b.pos-a.pos,c.pos-a.pos), (a.norm+b.norm+c.norm)*0.333f))
							std::swap(triangle.b, triangle.c);

						// append
						scene.triangles.push_back(triangle);
					}
				}
				else {

					if (subdiv_type_patches) {
						// Add "dummy triangles" to the scene representing the extent of the patches.
						// These are used to identify and include the second level patch BVHs when
						// building the first level BVH
						auto &patches = o.mesh.patches;
						int patch_offset = scene.patches.size();
						for (int p = 0; p < patches.size(); p++) {
							auto &patch = patches[p];

							const auto &root_box = patch.root_box;
							scene.scene_bounds.grow(root_box);

							triangle dummy_tri;
							vertex v;
							v.pos = root_box.min;
							scene.vertices.push_back(v);
							dummy_tri.a = scene.vertices.size()-1;

							v.pos = root_box.max;
							scene.vertices.push_back(v);
							dummy_tri.b = scene.vertices.size()-1;

							// Add again, because only extent of the volume is relevant, not the tri itself
							scene.vertices.push_back(v);
							dummy_tri.c = scene.vertices.size()-1;

							dummy_tri.material_id = ((uint32_t)-1) - (patch_offset + p); // reference to the patch id

							scene.triangles.push_back(dummy_tri);
						}

						// Store patches into scene
						scene.patches.insert(scene.patches.end(), patches.begin(), patches.end());
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
							scene.vertices.push_back(vertex);
							scene.scene_bounds.grow(vertex.pos);
						}

						for (uint32_t i = 0; i < serialized_verts.size(); i+=3) {
							triangle triangle;
							triangle.material_id = o.mesh.faces[i/3].material_id;

							triangle.a = index_offset + i;
							triangle.b = index_offset + i+1;
							triangle.c = index_offset + i+2;
							// test if geom normal agrees with shading normals
							// if not, flip winding order
							auto a = scene.vertices[triangle.a];
							auto b = scene.vertices[triangle.b];
							auto c = scene.vertices[triangle.c];
							if (!same_hemisphere(cross(b.pos-a.pos,c.pos-a.pos), (a.norm+b.norm+c.norm)*0.333f))
								std::swap(triangle.b, triangle.c);

							// append
							scene.triangles.push_back(triangle);
						}
					}
				}
			}
			else {
				std::cout << "No mesh: " << prim.GetPath() << std::endl;
			}
		}
	}
}