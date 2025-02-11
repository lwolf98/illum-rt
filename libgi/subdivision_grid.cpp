#include "subdivision.h"
#include <glm/ext.hpp>

using namespace subd;

void subd_patch::print_verts() {
	uint32_t length = len();
	for (uint32_t y = 0; y < length; ++y) {
		for (uint32_t x = 0; x < length; ++x) {
			//std::printf("(%d/%d) ", x, y);
			std::cout << "(" << verts[y*length+x] << ") ";
		}
		std::cout << std::endl;
	}
}

uint32_t subd_patch::len() {
	return std::pow(2, subd_level)+1;
}

// TODO: check for illegal operations?
// e.g. call vert_right on a vert that lies on the right edge...
uint32_t subd_patch::vert_right(uint32_t vert_id) {
	return vert_id+1;
}

uint32_t subd_patch::vert_down(uint32_t vert_id) {
	return vert_id + len();
}

uint32_t subd_patch::vert_down_right(uint32_t vert_id) {
	return vert_id + len() + 1;
}
