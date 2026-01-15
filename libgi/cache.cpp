#include "cache.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <glm/glm.hpp>

namespace subd {
	namespace cache {

		/*void store_model(const load_config &config, const std::filesystem::path &cache_path) {
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
		}*/
	}
}