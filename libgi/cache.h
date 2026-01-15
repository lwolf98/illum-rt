#pragma once

#include "load.h"
#include "subdivision.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <glm/glm.hpp>
#include <sstream>
#include <unordered_map>

#include <cstdint>
#include <type_traits>

namespace subd {
	namespace cache {
		void store_patch(std::ostream out) {

		}

		struct subd_patch_cache {
			uint32_t vert_count;
			uint32_t node_count;
			uint32_t subpatch_count;
			uint32_t material_id;
			uint32_t subd_level;
			int32_t  align_level;
			uint8_t  align_boxes; // NOT bool
		};

		struct subd_subpatch_cache {
			uint32_t node_count;
			uint32_t vert_start;
			uint32_t subd_level;
		};

		void write_subd_patch(std::ofstream& file, const subd_patch& p)
		{
			// ---- header ----
			subd_patch_cache header {
				static_cast<uint32_t>(p.verts.size()),
				static_cast<uint32_t>(p.nodes.size()),
				static_cast<uint32_t>(p.subpatches.size()),
				p.material_id,
				p.subd_level,
				p.align_level,
				p.align_boxes ? 1u : 0u
			};

			file.write(reinterpret_cast<char*>(&header), sizeof(header));

			// ---- flat data ----
			file.write(reinterpret_cast<const char*>(p.verts.data()),
					sizeof(vertex) * header.vert_count);

			file.write(reinterpret_cast<const char*>(p.nodes.data()),
					sizeof(patch_base_node) * header.node_count);

			// ---- subpatches ----
			for (const subd_subpatch& sp : p.subpatches) {
				subd_subpatch_cache sp_header {
					static_cast<uint32_t>(sp.nodes.size()),
					sp.vert_start,
					sp.subd_level
				};

				file.write(reinterpret_cast<char*>(&sp_header), sizeof(sp_header));

				file.write(reinterpret_cast<const char*>(sp.nodes.data()),
						sizeof(patch_base_node) * sp_header.node_count);

				file.write(reinterpret_cast<const char*>(&sp.trafo), sizeof(sp.trafo));
				file.write(reinterpret_cast<const char*>(&sp.proj), sizeof(sp.proj));
				file.write(reinterpret_cast<const char*>(&sp.root_box), sizeof(sp.root_box));
				file.write(reinterpret_cast<const char*>(&sp.root_box_world), sizeof(sp.root_box_world));
			}

			// ---- remaining fixed data ----
			file.write(reinterpret_cast<const char*>(&p.root_box), sizeof(p.root_box));
			file.write(reinterpret_cast<const char*>(p.data), sizeof(p.data));
		}

		void write_subd_patches(std::ofstream& file, const std::vector<subd_patch>& patches) {
			uint32_t patch_count =
				static_cast<uint32_t>(patches.size());

			file.write(reinterpret_cast<const char*>(&patch_count),
					sizeof(patch_count));

			for (const subd_patch& p : patches) {
				write_subd_patch(file, p);
			}
		}

		void store_model(const load_config& config, const std::filesystem::path& cache_path, const subd::object &obj) {
			std::string file_name = "test.cache";
			std::ofstream file(cache_path / file_name);
			if (!file.is_open()) {
				return; // caching not available...
				//throw std::runtime_error("Failed to open config file for writing");
			}

			file << "model_path = " << config.model_path.string() << '\n';
			file << "name = " << config.name << '\n';

			// glm::mat4 (row-major output)
			file << "model_matrix = ";
			for (int row = 0; row < 4; ++row) {
				for (int col = 0; col < 4; ++col) {
					file << config.model_matrix[col][row];
					if (!(row == 3 && col == 3))
						file << ' ';
				}
			}
			file << '\n';

			file << "subd_level = " << config.subd_level << '\n';
			file << "subd_type_patches = "
				<< (config.subd_type_patches ? "true" : "false") << '\n';

			file << "displacement_map = " << config.displacement_map << '\n';
			file << "displacement_strength = "
				<< config.displacement_strength << '\n';

			file << "bvh_align_level = " << config.bvh_align_level << '\n';

			file << "END OF HEADER" << '\n';

			//for (const auto &patch : obj.mesh.patches)
			//	write_subd_patch(file, patch);
			
			write_subd_patches(file, obj.mesh.patches);
		}

		void read_subd_patch(std::ifstream& file, subd_patch& p) {
			// ---- header ----
			subd_patch_cache header;
			file.read(reinterpret_cast<char*>(&header), sizeof(header));

			p.material_id  = header.material_id;
			p.subd_level   = header.subd_level;
			p.align_level  = header.align_level;
			p.align_boxes  = header.align_boxes != 0;

			// ---- flat data ----
			p.verts.resize(header.vert_count);
			file.read(reinterpret_cast<char*>(p.verts.data()),
					sizeof(vertex) * header.vert_count);

			p.nodes.resize(header.node_count);
			file.read(reinterpret_cast<char*>(p.nodes.data()),
					sizeof(patch_base_node) * header.node_count);

			// ---- subpatches ----
			p.subpatches.resize(header.subpatch_count);
			for (subd_subpatch& sp : p.subpatches) {
				subd_subpatch_cache sp_header;
				file.read(reinterpret_cast<char*>(&sp_header), sizeof(sp_header));

				sp.vert_start = sp_header.vert_start;
				sp.subd_level = sp_header.subd_level;

				sp.nodes.resize(sp_header.node_count);
				file.read(reinterpret_cast<char*>(sp.nodes.data()),
						sizeof(patch_base_node) * sp_header.node_count);

				file.read(reinterpret_cast<char*>(&sp.trafo), sizeof(sp.trafo));
				file.read(reinterpret_cast<char*>(&sp.proj), sizeof(sp.proj));
				file.read(reinterpret_cast<char*>(&sp.root_box), sizeof(sp.root_box));
				file.read(reinterpret_cast<char*>(&sp.root_box_world), sizeof(sp.root_box_world));
			}

			// ---- remaining fixed data ----
			file.read(reinterpret_cast<char*>(&p.root_box), sizeof(p.root_box));
			file.read(reinterpret_cast<char*>(p.data), sizeof(p.data));
		}

		void read_subd_patches(std::ifstream& file, std::vector<subd_patch>& patches) {
			uint32_t patch_count;
			file.read(reinterpret_cast<char*>(&patch_count),
					sizeof(patch_count));

			patches.resize(patch_count);

			//for (subd_patch& p : patches) {
			for (uint32_t i = 0; i < patch_count; ++i) {
				auto &patch = patches[i];
				read_subd_patch(file, patch);
			}
		}

		bool load_model(const std::filesystem::path& file_path, load_config& config) {
			std::ifstream file(file_path);
			if (!file.is_open()) {
				return false;
			}

			std::string line;
			while (std::getline(file, line)) {
				if (line.empty())
					continue;

				if (line == "END OF HEADER")
					break;

				auto eq_pos = line.find('=');
				if (eq_pos == std::string::npos)
					continue;

				std::string key   = line.substr(0, eq_pos);
				std::string value = line.substr(eq_pos + 1);

				// trim spaces
				auto trim = [](std::string& s) {
					size_t start = s.find_first_not_of(" \t");
					size_t end   = s.find_last_not_of(" \t");
					if (start == std::string::npos) {
						s.clear();
						return;
					}
					s = s.substr(start, end - start + 1);
				};

				trim(key);
				trim(value);

				if (key == "model_path") {
					config.model_path = value;
				}
				else if (key == "name") {
					config.name = value;
				}
				else if (key == "model_matrix") {
					std::istringstream iss(value);
					for (int row = 0; row < 4; ++row) {
						for (int col = 0; col < 4; ++col) {
							iss >> config.model_matrix[col][row];
						}
					}
				}
				else if (key == "subd_level") {
					config.subd_level = static_cast<uint32_t>(std::stoul(value));
				}
				else if (key == "subd_type_patches") {
					config.subd_type_patches =
						(value == "true" || value == "1");
				}
				else if (key == "displacement_map") {
					config.displacement_map = value;
				}
				else if (key == "displacement_strength") {
					config.displacement_strength = std::stof(value);
				}
				else if (key == "bvh_align_level") {
					config.bvh_align_level = std::stoi(value);
				}
				// unknown keys are ignored (forward-compatible)
			}

			std::vector<subd::subd_patch> patches;
			read_subd_patches(file, patches);

			return true;
		}

	}
}