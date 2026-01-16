#pragma once

#include "driver/defines.h"
#include "load.h"
#include "subdivision.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <glm/glm.hpp>
#include <sstream>
#include <unordered_map>
#include <iostream> // only DBG

#include <cstdint>
#include <type_traits>

namespace subd {
	namespace cache {
		struct subd_patch_cache {
			uint32_t vert_count;
			uint32_t node_count;
			uint32_t subpatch_count;
			uint32_t material_id;
			uint32_t subd_level;
			int32_t  align_level;
			uint32_t  align_boxes;
		};

		struct subd_subpatch_cache {
			uint32_t node_count;
			uint32_t vert_start;
			uint32_t subd_level;
		};

		// FNV-1a hashing (64-bit) https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
		using hash64 = uint64_t;
		static constexpr hash64 FNV_OFFSET = 14695981039346656037ull; // 0xcbf29ce484222325
		static constexpr hash64 FNV_PRIME  = 1099511628211ull; // 0x100000001b3
		static constexpr uint32_t approx_var = APPROXIMATION_VARIANT;
		static constexpr uint32_t comp_var = COMPRESSION_VARIANT;

		inline void hash_combine(hash64& h, const void* data, size_t size) {
			const uint8_t* bytes = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; ++i) {
				h ^= bytes[i];
				h *= FNV_PRIME;
			}
		}

		inline hash64 hash_config(const load_config& c) {
			hash64 h = FNV_OFFSET;

			// model identity
			const std::string path = c.model_path.string();
			hash_combine(h, path.data(), path.size());

			// geometry-affecting parameters
			hash_combine(h, &c.subd_level, sizeof(c.subd_level));
			hash_combine(h, &c.subd_type_patches, sizeof(c.subd_type_patches));
			hash_combine(h, &c.displacement_strength, sizeof(c.displacement_strength));
			hash_combine(h, &c.bvh_align_level, sizeof(c.bvh_align_level));

			// matrix
			hash_combine(h, &c.model_matrix, sizeof(c.model_matrix));

			// displacement map string
			hash_combine(h,
				c.displacement_map.data(),
				c.displacement_map.size());

			return h;
		}

		inline std::string hash_to_hex(uint64_t h) {
			std::ostringstream oss;
			oss << std::hex << std::setw(16) << std::setfill('0') << h;
			return oss.str();
		}

		inline std::string file_name(const load_config &config) {
			std::string hash = subd::cache::hash_to_hex(subd::cache::hash_config(config));
			return "subd_app" + std::to_string(approx_var) + "_comp" + std::to_string(comp_var) + "_" + hash + ".cache";
		}

		inline void write_subd_patch(std::ostream& file, const subd_patch& p)
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

			file.write(reinterpret_cast<const char*>(&header), sizeof(header));

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

				file.write(reinterpret_cast<const char*>(&sp_header), sizeof(sp_header));

#if defined(SLAB_COMPRESSION) || defined(QUANTIZATION)
				file.write(reinterpret_cast<const char*>(sp.nodes.data()),
						sizeof(patch_slab_node) * sp_header.node_count);
#else
				file.write(reinterpret_cast<const char*>(sp.nodes.data()),
						sizeof(patch_base_node) * sp_header.node_count);
#endif

				file.write(reinterpret_cast<const char*>(&sp.trafo), sizeof(sp.trafo));
				file.write(reinterpret_cast<const char*>(&sp.proj), sizeof(sp.proj));
				file.write(reinterpret_cast<const char*>(&sp.root_box), sizeof(sp.root_box));
				file.write(reinterpret_cast<const char*>(&sp.root_box_world), sizeof(sp.root_box_world));
			}

			// ---- remaining fixed data ----
			file.write(reinterpret_cast<const char*>(&p.root_box), sizeof(p.root_box));
			file.write(reinterpret_cast<const char*>(p.data), sizeof(p.data));
		}

		inline void write_subd_patches(std::ostream& file, const std::vector<subd_patch>& patches) {
			uint32_t patch_count =
				static_cast<uint32_t>(patches.size());

			file.write(reinterpret_cast<const char*>(&patch_count),
					sizeof(patch_count));

			for (const subd_patch& p : patches) {
				write_subd_patch(file, p);
			}
		}

		inline void store_model(const load_config& config, const std::filesystem::path& cache_path, const subd::object &obj) {
			std::cout << "Hash (store): " << hash_config(config) << std::endl;
			//std::string hash = subd::cache::hash_to_hex(subd::cache::hash_config(config));
			//std::string file_name = "subd_app" + std::to_string(approx_var) + "_comp" + std::to_string(comp_var) + "_" + hash + ".cache";
			std::string filename = file_name(config);
			std::ofstream file(cache_path / filename, std::ios::binary);
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

		inline void read_subd_patch(std::istream& file, subd_patch& p) {
			// ---- header ----
			subd_patch_cache header {};
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
				subd_subpatch_cache sp_header {};
				file.read(reinterpret_cast<char*>(&sp_header), sizeof(sp_header));

				sp.vert_start = sp_header.vert_start;
				sp.subd_level = sp_header.subd_level;

				sp.nodes.resize(sp_header.node_count);
#if defined(SLAB_COMPRESSION) || defined(QUANTIZATION)
				file.read(reinterpret_cast<char*>(sp.nodes.data()),
						sizeof(patch_slab_node) * sp_header.node_count);
#else
				file.read(reinterpret_cast<char*>(sp.nodes.data()),
						sizeof(patch_base_node) * sp_header.node_count);
#endif

				file.read(reinterpret_cast<char*>(&sp.trafo), sizeof(sp.trafo));
				file.read(reinterpret_cast<char*>(&sp.proj), sizeof(sp.proj));
				file.read(reinterpret_cast<char*>(&sp.root_box), sizeof(sp.root_box));
				file.read(reinterpret_cast<char*>(&sp.root_box_world), sizeof(sp.root_box_world));
			}

			// ---- remaining fixed data ----
			file.read(reinterpret_cast<char*>(&p.root_box), sizeof(p.root_box));
			file.read(reinterpret_cast<char*>(p.data), sizeof(p.data));
		}

		inline void read_subd_patches(std::istream& file, std::vector<subd_patch>& patches) {
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

		inline bool load_model(const std::filesystem::path& cache_path, const std::string &filename, load_config& config, std::vector<subd::subd_patch> &patches) {
			//std::string filename = file_name(config);
			std::filesystem::path file_path = cache_path / filename;
			std::ifstream file(file_path, std::ios::binary);
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
			std::cout << "Hash (load): " << hash_config(config) << std::endl;

			read_subd_patches(file, patches);

			return true;
		}
	}
}