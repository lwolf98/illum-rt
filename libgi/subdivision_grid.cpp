#include "subdivision.h"
#include <glm/ext.hpp>
#include <iomanip>

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

uint32_t decode_morton(uint morton) {
	uint32_t x = morton & 0x55555555;
	x = (x | (x >> 1)) & 0x33333333;
	x = (x | (x >> 2)) & 0x0F0F0F0F;
	x = (x | (x >> 4)) & 0x00FF00FF;
	x = (x | (x >> 8)) & 0x0000FFFF;
	return x;
}

void subd_patch::build_bvh() {
	//TODO: pre-allocate, e.g. level 3: 1 + 4 + 16 + 64 (2 children: (1+2) + (4+8) + (16+32) + (64))
	
	int nodes_count = geometric_series(subd_level-1, 4) + pow(4, subd_level);
	nodes.resize(nodes_count);
	if (subd_level == 0)
		std::cout << "";

	int size = (len()-1)*(len()-1);
	int off_children = geometric_series(subd_level-1, 4);
	for (uint32_t morton = 0; morton < size; ++morton) {
		uint32_t x = decode_morton(morton);
		uint32_t y = decode_morton(morton >> 1);

		uint32_t vert_index = y*len()+x;
		patch_node current_node;
		aabb box;
		box.grow(verts[vert_index].pos);
		box.grow(verts[vert_right(vert_index)].pos);
		box.grow(verts[vert_down(vert_index)].pos);
		box.grow(verts[vert_down_right(vert_index)].pos);
		current_node.box = box;

		uint32_t morton_code = calculate_morton_code(x, y);
		current_node.patch_ref = morton_code;
		assert(morton_code <= verts.size());
		nodes[off_children+morton] = current_node;
	}

	int off = 0;
	for (int i = 1; i <= subd_level; i++) {
		int len = pow(2,(subd_level-i));
		size = len*len;
		if (i > 1) {
			off_children = off;
		}
		off = geometric_series(subd_level-i-1, 4);

		for (uint32_t i = 0; i < size; ++i) {
			uint32_t index = i*4;

			patch_node current_node;

			patch_node &lower_node_1 = nodes[off_children + index];
			patch_node &lower_node_2 = nodes[off_children + index+1];
			patch_node &lower_node_3 = nodes[off_children + index+2];
			patch_node &lower_node_4 = nodes[off_children + index+3];

			current_node.box.grow(lower_node_1.box);
			current_node.box.grow(lower_node_2.box);
			current_node.box.grow(lower_node_3.box);
			current_node.box.grow(lower_node_4.box);

			current_node.node_1 = off_children + index;		// upper left
			current_node.node_2 = off_children + index+1;	// upper right
			current_node.node_3 = off_children + index+2;	// lower left
			current_node.node_4 = off_children + index+3;	// lower right

			nodes[off+i] = current_node;
		}
	}

	//if (subd_level == 0)
	//	nodes[0].set_subd_root_and_leaf();
		
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
