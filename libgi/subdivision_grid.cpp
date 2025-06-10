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
			assert(morton_code <= verts.size());
			nodes.emplace_back(current_node);
		}
	}

	for (int i = 0; i < subd_level; i++) {
		int len = pow(2,(subd_level-i));
		int off = 0;
		if (i > 0) {
			off = nodes_count - 3 * geometric_series(subd_level-i, 4) + 2;
		}
		for (int y_base = 0; y_base < len; y_base+=2) {
			for (int x_base = 0; x_base < len; x_base+=2) {
				int step = 1;
				if (i > 0)
					step = 3;
				int x = x_base * step;
				int y = y_base * step;

				node current_node;
				node node_l;
				node node_r;

				node &lower_node_1 = nodes[off + y*len+x];
				node &lower_node_2 = nodes[off + y*len+(x+step)];
				node &lower_node_3 = nodes[off + (y+step)*len+x];
				node &lower_node_4 = nodes[off + (y+step)*len+(x+step)];

				node_l.box.grow(lower_node_1.box);
				node_l.box.grow(lower_node_2.box);
				node_l.left = off + y*len+x;
				node_l.right = off + y*len+(x+step);
				nodes.emplace_back(node_l);
				current_node.box.grow(node_l.box);
				current_node.left = nodes.size()-1;

				node_r.box.grow(lower_node_3.box);
				node_r.box.grow(lower_node_4.box);
				node_r.left = off + (y+step)*len+x;
				node_r.right = off + (y+step)*len+(x+step);
				nodes.emplace_back(node_r);
				current_node.box.grow(node_r.box);
				current_node.right = nodes.size()-1;

				nodes.emplace_back(current_node);

				//if (i == subd_level-1)
				//	bvh_node = nodes.size()-1;

			}
		}
	}

	bvh_node = nodes.size()-1;
	if (subd_level == 0)
		nodes[bvh_node].set_subd_root_and_leaf();
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
