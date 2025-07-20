#include "subdivision.h"
#include <glm/ext.hpp>
#include <iomanip>
#include <iostream>

using namespace subd;

void subd_patch::print_verts() const {
	using namespace std;

	cout << "Vert positions:" << endl;
	cout << fixed << setprecision(5);

	uint32_t length = len();
	for (uint32_t y = 0; y < length; ++y) {
		for (uint32_t x = 0; x < length; ++x) {
			const glm::vec3 &pos = verts[y*length+x].pos;
			cout << "(" << setw(8) << pos.y << " " << setw(8) << pos.z << ") ";
		}
		cout << endl;
	}
}

void subd_patch::print_vert_tcs() const {
	using namespace std;

	cout << "Vert TCs:" << endl;
	cout << fixed << setprecision(5);

	uint32_t length = len();
	for (uint32_t y = 0; y < length; ++y) {
		for (uint32_t x = 0; x < length; ++x) {
			const glm::vec2 &tc = verts[y*length+x].tc;
			cout << "(" << setw(8) << tc.x << " " << setw(8) << tc.y << ") ";
		}
		cout << endl;
	}
}

uint32_t subd_patch::len(uint32_t level) const {
	return std::pow(2, level)+1;
}

uint32_t subd_patch::len() const {
	return len(subd_level);
}

// TODO: check for illegal operations?
// e.g. call vert_right on a vert that lies on the right edge...
uint32_t subd_patch::vert_right(uint32_t vert_id) const {
	return vert_id+1;
}

uint32_t subd_patch::vert_down(uint32_t vert_id) const {
	return vert_id + len();
}

uint32_t subd_patch::vert_down_right(uint32_t vert_id) const {
	return vert_id + len() + 1;
}

uint32_t subd_patch::vert_offset(uint32_t vert_id, int32_t off_x, int32_t off_y) const {
	return vert_id + off_y * len() + off_x;
}

//TODO: explain reason for passing iterations = -1 (happens on subd_level = 0)
int geometric_series(int iterations, int base) {
	return (1-pow(base, iterations+1))/(1-base);
}

//TODO: find better place, maybe inside of subd_patch?
//TODO: remove one of the variables (x, morton)
uint32_t decode_morton(uint32_t morton) {
	uint32_t x = morton & 0x55555555;
	x = (x | (x >> 1)) & 0x33333333;
	x = (x | (x >> 2)) & 0x0F0F0F0F;
	x = (x | (x >> 4)) & 0x00FF00FF;
	x = (x | (x >> 8)) & 0x0000FFFF;
	return x;
}

void subd_patch::build_bvh() {
	//TODO: pre-allocate, e.g. level 4: 1 + 4 + 16 + 64 = 85
	
	int nodes_count = geometric_series(subd_level-1, 4);
	nodes.resize(nodes_count);

	int off_children = geometric_series(subd_level-2, 4);
	int size = (len()-1)*(len()-1);
	for (uint32_t morton = 0; morton < size; ++morton) {
		uint32_t x = decode_morton(morton);
		uint32_t y = decode_morton(morton >> 1);

		uint32_t vert_index = y*len()+x;
		aabb box;
		box.grow(verts[vert_index].pos);
		box.grow(verts[vert_right(vert_index)].pos);
		box.grow(verts[vert_down(vert_index)].pos);
		box.grow(verts[vert_down_right(vert_index)].pos);

		if (subd_level > 0)	nodes[off_children+(morton>>2)].boxes[morton%4] = box;
		else				{ root_box = box; return; }
	}

	int off = 0;
	for (int i = 1; i <= subd_level; i++) {
		int len = pow(2,(subd_level-i));
		size = len*len;
		if (i > 1)			off_children = off;
		if (i < subd_level)	off = geometric_series(subd_level-i-2, 4);

		for (uint32_t j = 0; j < size; ++j) {
			const patch_node &child_node = nodes[off_children + j];
			aabb box;
			box.grow(child_node.boxes[0]);
			box.grow(child_node.boxes[1]);
			box.grow(child_node.boxes[2]);
			box.grow(child_node.boxes[3]);

			if (i < subd_level)	nodes[off+(j>>2)].boxes[j%4] = box;
			else				root_box = box;
		}
	}	
}

int subd_patch::calculate_morton_code(int x, int y) const {
	// Note: currently only working with index, is morton code neccessary?
	return y * len() + x;
}

tuple<int, int> subd_patch::evaluate_morton_code(int morton_code) const {
	// Note: currently only working with index, is morton code neccessary?
	int x = morton_code % len();
	int y = morton_code / len();
	return {x, y};
}

int subd_patch::get_subd_quad(int morton_code) const {
	auto [x, y] = evaluate_morton_code(morton_code);
	return y * len() + x;
}

std::array<triangle, 2> subd_patch::tris(int morton_code) const {
	return {
		tri(morton_code, true),
		tri(morton_code, false)
	};
}

triangle subd_patch::tri(int morton_code, bool upper) const {
	triangle tri;

	//TODO:
	// get subd quad by morton code rather than the currently used position code?
	int quad_id = get_subd_quad(morton_code);
	tri.material_id = material_id;
	if (upper) {
		tri.a = quad_id;
		tri.b = vert_down(quad_id);
		tri.c = vert_right(quad_id);
	}
	else {
		tri.a = vert_down(quad_id);
		tri.b = vert_down_right(quad_id);
		tri.c = vert_right(quad_id);
	}

	return tri;
}

uint32_t subd_patch::quad_ref_from_index(uint32_t index) const {
	uint32_t x = decode_morton(index);
	uint32_t y = decode_morton(index >> 1);
	return y*len() + x;
}
