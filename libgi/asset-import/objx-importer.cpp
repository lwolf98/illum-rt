#include "asset-import.h"

#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <utility>
#include <cstdint>
#include <iostream>
#include <glm/vec3.hpp>

namespace import {

	// logic created with ChatGPT
	//std::map<std::pair<int,int>, float> read_creases(const std::string& file_path) {
	/*subd::edge_list read_creases(const std::string& file_path) {
		subd::edge_list creases;
		//std::map<std::pair<int,int>, float> crease_map;
		std::ifstream file(file_path);
		if (!file) {
			std::cerr << "Failed to open file: " << file_path << std::endl;
			//return crease_map;
			return creases;
		}

		std::string line;

		while (std::getline(file, line)) {
			std::stringstream ss(line);

			std::string t_word;
			std::string crease_word;
			std::string tag;   // holds "2/1/0"
			int vertex_1;
			int vertex_2;
			float weight;

			if (ss >> t_word >> crease_word >> tag >> vertex_1 >> vertex_2 >> weight) {
				if (t_word == "t" && crease_word == "crease" && tag == "2/1/0") {
					//crease_map[{vertex_1, vertex_2}] = weight;
					//creases.add(vertex_1, vertex_2, weight);
					creases.add(vertex_1, vertex_2, weight);
				}
			}
		}

		//return crease_map;
		return creases;
	}*/

	// logic created with ChatGPT
	subd::crease_mapping read_crease_mapping(const std::string& file_path) {
		subd::crease_mapping result;

		std::ifstream file(file_path);
		if (!file) {
			std::cerr << "Failed to open file: " << file_path << std::endl;
			return result;
		}

		std::string line;
		uint32_t vertex_index = 0; // OBJ indices are 1-based

		while (std::getline(file, line)) {
			std::stringstream ss(line);

			std::string first_token;
			if (!(ss >> first_token))
				continue;

			// Vertex line: v x y z
			if (first_token == "v") {
				float x = 0.0f;
				float y = 0.0f;
				float z = 0.0f;

				if (ss >> x >> y >> z) {
					result.vertex_map[vertex_index] = glm::vec3{x, y, z};
					++vertex_index;
				}

				continue;
			}

			// Crease line: t crease 2/1/0 i j w
			if (first_token == "t") {
				std::string crease_word;
				std::string tag;
				int vertex_1 = 0;
				int vertex_2 = 0;
				float weight = 0.0f;

				if (ss >> crease_word >> tag >> vertex_1 >> vertex_2 >> weight) {
					if (crease_word == "crease" && tag == "2/1/0") {
						result.creases[{vertex_1, vertex_2}] = weight/10.f;
					}
				}

				continue;
			}
		}

		return result;
	}



	void objx_importer::load_scene() {
		//stage = UsdStage::Open(filepath);
		assimp_importer::load_scene();
	}

	void objx_importer::import(scene& scene) {
		subd::crease_mapping creases = read_crease_mapping(cfg.model_path);
		assimp_importer::import(scene, creases);
	}
}