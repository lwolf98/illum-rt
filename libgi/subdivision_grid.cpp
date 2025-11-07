#include "subdivision.h"
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iomanip>
#include <iostream>
#include <fstream>

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
	//return std::pow(2, level)+1;
	return (1 << level) + 1; // 2^subd_level + 1
}

uint32_t subd_patch::len() const {
	return len(subd_level);
}

// TODO: check for illegal operations?
// e.g. call vert_right on a vert that lies on the right edge...
uint32_t subd_patch::vert_right(uint32_t vert_id, uint32_t step) const {
	return vert_id+step;
}

uint32_t subd_patch::vert_down(uint32_t vert_id, uint32_t step) const {
	return vert_id + step*len();
}

uint32_t subd_patch::vert_down_right(uint32_t vert_id, uint32_t step) const {
	return vert_id + step*len() + step;
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

class bvh_writer {
	std::string path;
	std::ofstream outfile;
	std::string name;
	uint32_t v_off;
	glm::mat3 M_trafo;
	uint32_t next_level;

public:
	std::string name_ext;

public:
	bvh_writer(std::string outfile_path, std::string name)
		: path(outfile_path), name(name),
		  next_level(0), v_off(0), M_trafo(1) { }

	~bvh_writer() {
		outfile.close();
	}

	void start_bvh() {
		if (next_level == 0) {
			outfile.open(path);
			outfile << "o " << name << name_ext << "_Level_" << next_level << std::endl;
			next_level++;
		}
	}

	void set_trafo(glm::mat3 trafo) {
		M_trafo = trafo;
	}

	void new_level() {
		if (next_level >= 1) {
			next_level++;
			new_object();
		}
	}

	void new_object() {
		outfile << "o " << name << name_ext << "_Level_" << (next_level-1) << std::endl;	
	}

	void set_level(uint32_t level) {
		outfile << "o " << name << name_ext << "_Level_" << level << std::endl;
		next_level = level+1;
	}

	void print_box(aabb box) {
		vec3 v_1 = M_trafo * vec3(box.min.x, box.min.y, box.min.z);
		vec3 v_2 = M_trafo * vec3(box.max.x, box.min.y, box.min.z);
		vec3 v_3 = M_trafo * vec3(box.max.x, box.max.y, box.min.z);
		vec3 v_4 = M_trafo * vec3(box.min.x, box.max.y, box.min.z);
		vec3 v_5 = M_trafo * vec3(box.min.x, box.min.y, box.max.z);
		vec3 v_6 = M_trafo * vec3(box.max.x, box.min.y, box.max.z);
		vec3 v_7 = M_trafo * vec3(box.max.x, box.max.y, box.max.z);
		vec3 v_8 = M_trafo * vec3(box.min.x, box.max.y, box.max.z);

		//outfile << "v " << box.min.x << " " << box.min.y << " " << box.min.z << std::endl;
		//outfile << "v " << box.max.x << " " << box.min.y << " " << box.min.z << std::endl;
		//outfile << "v " << box.max.x << " " << box.max.y << " " << box.min.z << std::endl;
		//outfile << "v " << box.min.x << " " << box.max.y << " " << box.min.z << std::endl;
		//outfile << "v " << box.min.x << " " << box.min.y << " " << box.max.z << std::endl;
		//outfile << "v " << box.max.x << " " << box.min.y << " " << box.max.z << std::endl;
		//outfile << "v " << box.max.x << " " << box.max.y << " " << box.max.z << std::endl;
		//outfile << "v " << box.min.x << " " << box.max.y << " " << box.max.z << std::endl;

		outfile << "v " << v_1.x << " " << v_1.y << " " << v_1.z << std::endl;
		outfile << "v " << v_2.x << " " << v_2.y << " " << v_2.z << std::endl;
		outfile << "v " << v_3.x << " " << v_3.y << " " << v_3.z << std::endl;
		outfile << "v " << v_4.x << " " << v_4.y << " " << v_4.z << std::endl;
		outfile << "v " << v_5.x << " " << v_5.y << " " << v_5.z << std::endl;
		outfile << "v " << v_6.x << " " << v_6.y << " " << v_6.z << std::endl;
		outfile << "v " << v_7.x << " " << v_7.y << " " << v_7.z << std::endl;
		outfile << "v " << v_8.x << " " << v_8.y << " " << v_8.z << std::endl;
		
		//outfile << "f " << "1 2 3 4" << std::endl;
		//outfile << "f " << "5 6 7 8" << std::endl;
		//outfile << "f " << "1 2 6 5" << std::endl;
		//outfile << "f " << "2 6 7 3" << std::endl;
		//outfile << "f " << "4 3 7 8" << std::endl;
		//outfile << "f " << "5 1 4 8" << std::endl;

		outfile << "f " << v_off+1 << " " << v_off+2 << " " << v_off+3 << " " << v_off+4 << std::endl;
		outfile << "f " << v_off+5 << " " << v_off+6 << " " << v_off+7 << " " << v_off+8 << std::endl;
		outfile << "f " << v_off+1 << " " << v_off+2 << " " << v_off+6 << " " << v_off+5 << std::endl;
		outfile << "f " << v_off+2 << " " << v_off+6 << " " << v_off+7 << " " << v_off+3 << std::endl;
		outfile << "f " << v_off+4 << " " << v_off+3 << " " << v_off+7 << " " << v_off+8 << std::endl;
		outfile << "f " << v_off+5 << " " << v_off+1 << " " << v_off+4 << " " << v_off+8 << std::endl;
		outfile << std::endl;

		v_off += 8;
	}
};

glm::mat3 trafo_matrix(vec3 a, vec3 b) {
	//glm::mat3 M_default(1);
	a = normalize(a);
	b = normalize(b);
	vec3 c = normalize(cross(a, b));
	vec3 n = normalize(cross(b, c));
	//glm::mat3 M_target(a, b, c);
	glm::mat3 M_target(b, c, n);
	glm::mat3 M_trafo = inverse(M_target); // * M_default;
	return M_trafo;
}

// logical operation:
// x - (x % n), given: n must be a power of 2
inline uint32_t truncate_to_block(uint32_t x, uint32_t n) {
	return x & ~(n-1);
	//return x - (x % n);
	//return x / n;
}

uint32_t subd_subpatch::len() const {
	//return std::pow(2, subd_level)+1;
	return (1 << subd_level) + 1; // 2^subd_level + 1
}

static bvh_writer *writer = nullptr;

void subd_subpatch::build_bvh(const subd_patch *parent, bool debug) {
	glm::mat3 &T = trafo;
	glm::mat3 T_inv = inverse(trafo);
	writer->set_trafo(T_inv);
	writer->set_level(0);
	const auto &verts = parent->verts;
	//uint32_t len = 1 << subd_level; // 2^subd_level
	//uint32_t block_step = block_len * block_len;
	int size = (len()-1)*(len()-1);
	
	int nodes_count = geometric_series(subd_level-1, 4);
	nodes.resize(nodes_count);

	int off_children = geometric_series(subd_level-2, 4);
	//int size = (len()-1)*(len()-1);
	for (uint32_t morton = 0; morton < size; ++morton) {
		uint32_t x = decode_morton(morton);
		uint32_t y = decode_morton(morton >> 1);
		//uint32_t x_block = truncate_to_block(x, block_len);
		//uint32_t y_block = truncate_to_block(y, block_len);

		uint32_t vert_index = vert_start + y*parent->len()+x;
		//uint32_t vert_index = y*len()+x;
		//uint32_t block_index = y_block*len()+x_block;

		//std::cout << "Trafo (" << morton << "):\t" << glm::to_string(T_inv) << std::endl;
		//writer->set_trafo(T_inv);
		
		aabb box;
		box.grow(T * verts[vert_index].pos);
		box.grow(T * verts[parent->vert_right(vert_index)].pos);
		box.grow(T * verts[parent->vert_down(vert_index)].pos);
		box.grow(T * verts[parent->vert_down_right(vert_index)].pos);

		if (debug)
			writer->print_box(box);

		if (subd_level > 0)
			nodes[off_children+(morton>>2)].boxes[morton%4] = box;
		else
			root_box = box; return; 

	}

	//writer.set_trafo(glm::mat3(1.f));

	int off = 0;
	for (int i = 1; i <= subd_level; i++) {
		if (debug)
			writer->new_level();

		int len = 1 << (subd_level-i); // 2^(subd_level-i);
		size = len*len;
		if (i > 1)			off_children = off;
		if (i < subd_level)	off = geometric_series(subd_level-i-2, 4);

		for (uint32_t j = 0; j < size; ++j) {
			const patch_node &child_node = nodes[off_children + j];
			aabb box;

			//glm::mat3 T_inv = child_node.trafos[0];
			//glm::mat3 T = inverse(T_inv);
			box.grow(child_node.boxes[0]);
			box.grow(child_node.boxes[1]);
			box.grow(child_node.boxes[2]);
			box.grow(child_node.boxes[3]);
			if (debug) {
				//writer->set_trafo(child_node.trafos[0]);
				writer->print_box(box);
			}

			if (i < subd_level)
				nodes[off+(j>>2)].boxes[j%4] = box;
			else
				root_box = box;
			
		}
	}
}

void subd_patch::build_bvh(int32_t align_level, bool debug) {
	if (debug) {
		writer = new bvh_writer("dbg_bvh/patch_x.obj", "S" + std::to_string(subd_level) + "_A" + std::to_string(align_level));
		writer->name_ext = "_aligned";
		writer->start_bvh();
	}

	// subd_level: overall level to subdivide to
	// align_level: subdivision level to start the object alignment
	// aligned_subd_level: level to subdivide to from the subpatch
	// relation: aligned_subd_level = subd_level - align_level
	/**
	 *	subd_level = 4, align_level = 3, aligned_subd_level = 1
	 * 	
	 * 	subd_level				_
	 * 	top		0	|___|_		 |
	 * 	level	1	|___|_		 |_ height = align_level
	 * 	________2___|___|_____  _|  ____________________
	 * 	bottom	3	|___|_		 |_ height = aligned_subd_level + 1
	 * 	level	4	|	|		_|
	 */

	this->align_boxes = align_level >= 0 && align_level <= subd_level;
	if (!align_boxes)
		align_level = subd_level;
	this->align_level = align_level;

	uint32_t blocks = 1 << 2 * align_level;				// 4^align_level
	if (align_boxes)
		subpatches.resize(blocks);

	int nodes_count = align_boxes ?
						geometric_series(align_level-1, 4)
					  : geometric_series(subd_level-1, 4);
	nodes.resize(nodes_count);

	/* Create subpatches and BL-BVHs */
	if (align_boxes) {
		uint32_t aligned_subd_level = subd_level - align_level;
		uint32_t block_size = 1 << 2*aligned_subd_level;	// 4^aligned_subd_level
		uint32_t block_len = 1 << aligned_subd_level;		// 2^aligned_subd_level

		for (uint32_t morton = 0; morton < blocks; morton++) {
			uint32_t block_start = morton * block_size;
			uint32_t x = decode_morton(block_start);
			uint32_t y = decode_morton(block_start >> 1);
			uint32_t vert_index = y*len()+x;

			// base from one corner
			/*glm::mat3 T = trafo_matrix(
					verts[vert_down(vert_index, block_len)].pos - verts[vert_index].pos,
					verts[vert_right(vert_index, block_len)].pos - verts[vert_index].pos
				);*/
			// base from averaged diagonales
			glm::mat3 T = trafo_matrix(
					verts[vert_down_right(vert_index, block_len)].pos - verts[vert_index].pos,
					verts[vert_down(vert_index, block_len)].pos - verts[vert_right(vert_index)].pos
				);
			//glm::mat3 T_inv = inverse(T); //TODO: equal to transpose here?

			subd_subpatch &sub = subpatches[morton];
			sub.vert_start = vert_index;
			sub.trafo = T;
			sub.subd_level = aligned_subd_level;
		}

		for (uint32_t morton = 0; morton < blocks; morton++)
			subpatches[morton].build_bvh(this, debug);

	}

	if (debug) {
		writer->set_trafo(glm::mat3(1.f));
		writer->name_ext = "_ax_aligned";
		writer->new_object();
	}

	
	/* Create first level of TL-BVH */
	int off_children = geometric_series(align_level-2, 4);
	uint32_t bottom_size = align_boxes ?
							subpatches.size()
						  : 1 << 2 * subd_level;
	for (uint32_t morton = 0; morton < bottom_size; ++morton) {
		aabb box;
		if (align_boxes) {
			// BVH with object-aligned boxes:

			subd_subpatch &sub = subpatches[morton];
			auto &T = sub.trafo;
			auto T_inv = inverse(T);

			box.grow(sub.root_box, T_inv);
		}
		else {
			// full aabb BVH:

			uint32_t x = decode_morton(morton);
			uint32_t y = decode_morton(morton >> 1);

			uint32_t vert_index = y*len()+x;

			box.grow(verts[vert_index].pos);
			box.grow(verts[vert_right(vert_index)].pos);
			box.grow(verts[vert_down(vert_index)].pos);
			box.grow(verts[vert_down_right(vert_index)].pos);
		}

		if (debug)
			writer->print_box(box);

		if (align_level > 0)
			nodes[off_children+(morton>>2)].boxes[morton%4] = box;
		else
			root_box = box; return;

	}


	/* Create upper levels of the TL-BVH */
	writer->set_trafo(glm::mat3(1.f));

	int off = 0;
	for (int i = 1; i <= align_level; i++) {
		if (debug) writer->new_level();

		int len = 1 << (align_level-i); // 2^(align_level-i);
		uint32_t size = len*len;
		if (i > 1)			off_children = off;
		if (i < align_level)	off = geometric_series(align_level-i-2, 4);

		for (uint32_t j = 0; j < size; ++j) {
			const patch_node &child_node = nodes[off_children + j];
			aabb box;
			box.grow(child_node.boxes[0]);
			box.grow(child_node.boxes[1]);
			box.grow(child_node.boxes[2]);
			box.grow(child_node.boxes[3]);
			if (debug) writer->print_box(box);

			if (i < align_level)	nodes[off+(j>>2)].boxes[j%4] = box;
			else					root_box = box;
		}
	}

	delete writer;
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

uint32_t subd_patch::quad_ref_from_index(uint32_t index, uint32_t level) const {
	uint32_t x = decode_morton(index);
	uint32_t y = decode_morton(index >> 1);
	return y*len(level) + x;
}

uint32_t subd_patch::quad_ref_from_index(uint32_t index) const {
	return quad_ref_from_index(index, subd_level);
}

uint32_t subd_patch::subpatch_ref_from_index(uint32_t index) const {
	uint32_t x = decode_morton(index);
	uint32_t y = decode_morton(index >> 1);
	return y*(len(align_level)-1) + x;
}