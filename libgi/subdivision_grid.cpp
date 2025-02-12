#include "subdivision.h"
#include <glm/ext.hpp>

using namespace subd;

void subd_patch::print_verts() {
	uint32_t length = len();
	for (uint32_t y = 0; y < length; ++y) {
		for (uint32_t x = 0; x < length; ++x) {
			//std::printf("(%d/%d) ", x, y);
			std::cout << "(" << verts[y*length+x].pos << ") ";
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

int geometric_series(int iterations, int base) {
	return (1-pow(base, iterations+1))/(1-base);
}

void subd_patch::build_bvh() {
	//TODO: pre-allocate, e.g. level 3: 1 + 4 + 16 + 64 (2 children: (1+2) + (4+8) + (16+32) + (64))
	
	int nodes_count = 3 * geometric_series(subd_level-1, 4) + pow(4, subd_level);
	nodes.reserve(nodes_count);
	//int offset = nodes.size() - pow(4, subd_level); //TODO: order nodes
	for (int y = 0; y < len()-1; y++) {
		for (int x = 0; x < len()-1; x++) {
			uint32_t vert_index = y*len()+x;
			node current_node;
			aabb box;
			box.grow(verts[vert_index].pos);
			box.grow(verts[vert_right(vert_index)].pos);
			box.grow(verts[vert_down(vert_index)].pos);
			box.grow(verts[vert_down_right(vert_index)].pos);
			current_node.box = box;

			uint32_t morton_code = calculate_morton_code(x, y); //0xffffff; //TODO: morton code
			//current_node.triangle = ((uint32_t)-1) - morton_code;
			current_node.set_secondary_value(morton_code);
			nodes.emplace_back(current_node);
		}
	}

	for (int i = 0; i < subd_level; i++) {
		int len = pow(2,(subd_level-i));
		int off = 3 * geometric_series(i, 4);
		for (int y = 0; y < len; y+=2) {
			for (int x = 0; x < len; x+=2) {

				node current_node;
				node node_l;
				node node_r;

				node &lower_node_1 = nodes[y*len+x];
				node &lower_node_2 = nodes[y*len+(x+1)];
				node &lower_node_3 = nodes[(y+1)*len+x];
				node &lower_node_4 = nodes[(y+1)*len+(x+1)];

				node_l.box.grow(lower_node_1.box);
				node_l.box.grow(lower_node_2.box);
				node_l.left = y*len+x;
				node_l.right = y*len+(x+1);
				nodes.emplace_back(node_l);
				current_node.box.grow(node_l.box);
				current_node.left = nodes.size()-1;

				node_r.box.grow(lower_node_3.box);
				node_r.box.grow(lower_node_4.box);
				node_r.left = (y+1)*len+x;
				node_r.right = (y+1)*len+(x+1);
				nodes.emplace_back(node_r);
				current_node.box.grow(node_r.box);
				current_node.right = nodes.size()-1;

				nodes.emplace_back(current_node);

				if (i == subd_level-1)
					bvh_node = nodes.size()-1;

			}
		}
	}
}

int subd_patch::calculate_morton_code(int x, int y) {
	// Note: currently only working with index, is morton code neccessary?
	return y * len() + x;
}

tuple<int, int> subd_patch::evaluate_morton_code(int morton_code) {
	// Note: currently only working with index, is morton code neccessary?
	int x = morton_code % len();
	int y = morton_code / len();
	return {x, y};
}

int subd_patch::get_subd_quad(int morton_code) {
	auto [x, y] = evaluate_morton_code(morton_code);
	return y * len() + x;
}

std::array<triangle, 2> subd_patch::tris(int morton_code) {
	triangle tri1;
	triangle tri2;

	//TODO:
	// get subd quad by morton code
	// get two tris from it
	int quad_id = get_subd_quad(morton_code);
	tri1.a = quad_id;
	tri1.b = vert_down(quad_id);
	tri1.c = vert_right(quad_id);
	tri1.material_id = material_id;

	tri2.a = vert_down(quad_id);
	tri2.b = vert_down_right(quad_id);
	tri2.c = vert_right(quad_id);
	tri2.material_id = material_id;

	return { tri1, tri2 };
}
