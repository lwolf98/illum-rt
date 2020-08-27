#include "scene.h"

#include "bvh.h"

#include <vector>
#include <iostream>
#include <map>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>
#include <assimp/material.h>

using namespace glm;
using namespace std;

inline vec3 to_glm(const aiVector3D& v) { return vec3(v.x, v.y, v.z); }

void scene::add(const std::string& filename, const std::string &name, const mat4 &trafo) {
    // load from disk
    Assimp::Importer importer;
    std::cout << "Loading: " << filename << "..." << std::endl;
	unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals;  // | aiProcess_FlipUVs  // TODO assimp
    const aiScene* scene_ai = importer.ReadFile(filename, flags);
    if (!scene_ai) // handle error
        throw std::runtime_error("ERROR: Failed to load file: " + filename + "!");

	// todo: store indices prior to adding anything to allow "transform-last"

	// load materials
    for (uint32_t i = 0; i < scene_ai->mNumMaterials; ++i) {
		::material material;
        aiString name_ai;
		aiColor3D col;
		auto mat_ai = scene_ai->mMaterials[i];
        mat_ai->Get(AI_MATKEY_NAME, name_ai);
		if (name != "")
			material.name = name + "/" + name_ai.C_Str();
		else
			material.name = name_ai.C_Str();
		if (mat_ai->Get(AI_MATKEY_COLOR_DIFFUSE, col) == AI_SUCCESS)
			material.kd = glm::vec4(col.r, col.g, col.b, 1.0f);
		if (mat_ai->Get(AI_MATKEY_COLOR_SPECULAR, col) == AI_SUCCESS)
			material.ks = glm::vec4(col.r, col.g, col.b, 1.0f);
		if (mat_ai->Get(AI_MATKEY_SHININESS, col.r) == AI_SUCCESS)
			material.ks.w = col.r;
		material.kd = pow(material.kd, vec4(2.2f, 2.2f, 2.2f, 1.0f));
		material.ks = pow(material.ks, vec4(2.2f, 2.2f, 2.2f, 1.0f));
		materials.push_back(material);
	}

    // load meshes
    for (uint32_t i = 0; i < scene_ai->mNumMeshes; ++i) {
        const aiMesh *mesh_ai = scene_ai->mMeshes[i];
		uint32_t material_id = scene_ai->mMeshes[i]->mMaterialIndex;
		uint32_t index_offset = vertices.size();
		
		for (uint32_t i = 0; i < mesh_ai->mNumVertices; ++i) {
			vertex vertex;
			vertex.pos = to_glm(mesh_ai->mVertices[i]);
			vertex.norm = to_glm(mesh_ai->mNormals[i]);
			if (mesh_ai->HasTextureCoords(0))
				vertex.tc = vec2(to_glm(mesh_ai->mTextureCoords[0][i]));
			else
				vertex.tc = vec2(0,0);
			vertices.push_back(vertex);
		}
 
		for (uint32_t i = 0; i < mesh_ai->mNumFaces; ++i) {
			const aiFace &face = mesh_ai->mFaces[i];
			if (face.mNumIndices == 3) {
				triangle triangle;
				triangle.a = face.mIndices[0] + index_offset;
				triangle.b = face.mIndices[1] + index_offset;
				triangle.c = face.mIndices[2] + index_offset;
				triangle.material_id = material_id;
				triangles.push_back(triangle);
			}
			else
				std::cout << "WARN: Mesh: skipping non-triangle [" << face.mNumIndices << "] face (that the ass imp did not triangulate)!" << std::endl;
		}
	}
}

