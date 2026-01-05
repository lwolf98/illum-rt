#include "subdivision.h"
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#ifdef PROJECTION
#include <eigen3/Eigen/Dense>
#endif
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
static int geometric_series4(int iterations) {
	return (1 - (1 << ((iterations+1)<<1))) / (-3);
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

uint32_t encode_morton(uint32_t x, uint32_t y) {
	auto spread_bits = [](uint32_t v) {
		v &= 0x0000FFFF; // clear upper bits
		v = (v | (v << 8)) & 0x00FF00FF;
		v = (v | (v << 4)) & 0x0F0F0F0F;
		v = (v | (v << 2)) & 0x33333333;
		v = (v | (v << 1)) & 0x55555555;
		return v;
	};

	return spread_bits(x) | (spread_bits(y) << 1);
}

#ifdef PROJECTION
inline glm::mat3 compute_homography(const std::vector<glm::vec2> input, const std::vector<glm::vec2> target) {
	using namespace Eigen;

	MatrixXd A(8, 8);
	VectorXd b(8);

	for (uint32_t i = 0; i < 4; ++i) {
		const glm::vec2 in = input[i];
		const glm::vec2 tar = target[i];
		A.row(0+i) << in.x,  in.y,  1.f,   0.f,   0.f,  0.f,  -tar.x*in.x,  -tar.x*in.y;
		A.row(4+i) <<  0.f,   0.f,  0.f,  in.x,  in.y,  1.f,  -tar.y*in.x,  -tar.y*in.y;

		b.row(0+i) << tar.x;
		b.row(4+i) << tar.y;
	}
	//std::cout << "matrix: " << A << std::endl;
	//std::cout << "result: " << b << std::endl;

	VectorXd x(8);
	x = A.fullPivLu().solve(b);
	//std::cout << "variables: " << x << std::endl;

	return glm::mat3 {
		x[0], x[3], x[6],	// column 0
		x[1], x[4], x[7],	// column 1
		x[2], x[5], 1.f		// column 2
	};
}

inline glm::vec2 xz(glm::vec3 xyz) {
	return glm::vec2(xyz.x, xyz.z);
}

inline glm::vec3 project(const glm::vec3 &a, const glm::mat3 &proj) {
	glm::vec3 tmp = proj * glm::vec3(a.x, a.z, 1.f);
	return glm::vec3(tmp.x/tmp.z, a.y, tmp.y/tmp.z);
}
#endif

class bvh_writer {
	std::string path;
	std::ofstream outfile;
	std::string name;
	uint32_t v_off;
	glm::mat3 M_trafo;
#ifdef PROJECTION
	glm::mat3 M_proj;
#endif
	uint32_t next_level;

public:
	std::string name_ext;

public:
	bvh_writer(const std::string &outfile_path, const std::string &name)
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

	void set_trafo(const glm::mat3 &trafo) {
		M_trafo = trafo;
	}

#ifdef PROJECTION
	void set_proj(const glm::mat3 &proj) {
		M_proj = proj;
	}
#endif

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

	void print_box(const aabb &box) {
#ifndef PROJECTION

		vec3 v_1 = M_trafo * vec3(box.min.x, box.min.y, box.min.z);
		vec3 v_2 = M_trafo * vec3(box.max.x, box.min.y, box.min.z);
		vec3 v_3 = M_trafo * vec3(box.max.x, box.max.y, box.min.z);
		vec3 v_4 = M_trafo * vec3(box.min.x, box.max.y, box.min.z);
		vec3 v_5 = M_trafo * vec3(box.min.x, box.min.y, box.max.z);
		vec3 v_6 = M_trafo * vec3(box.max.x, box.min.y, box.max.z);
		vec3 v_7 = M_trafo * vec3(box.max.x, box.max.y, box.max.z);
		vec3 v_8 = M_trafo * vec3(box.min.x, box.max.y, box.max.z);
#else
		vec3 v_1 = M_trafo * project(vec3(box.min.x, box.min.y, box.min.z), M_proj);
		vec3 v_2 = M_trafo * project(vec3(box.max.x, box.min.y, box.min.z), M_proj);
		vec3 v_3 = M_trafo * project(vec3(box.max.x, box.max.y, box.min.z), M_proj);
		vec3 v_4 = M_trafo * project(vec3(box.min.x, box.max.y, box.min.z), M_proj);
		vec3 v_5 = M_trafo * project(vec3(box.min.x, box.min.y, box.max.z), M_proj);
		vec3 v_6 = M_trafo * project(vec3(box.max.x, box.min.y, box.max.z), M_proj);
		vec3 v_7 = M_trafo * project(vec3(box.max.x, box.max.y, box.max.z), M_proj);
		vec3 v_8 = M_trafo * project(vec3(box.min.x, box.max.y, box.max.z), M_proj);
#endif

		outfile << "v " << v_1.x << " " << v_1.y << " " << v_1.z << std::endl;
		outfile << "v " << v_2.x << " " << v_2.y << " " << v_2.z << std::endl;
		outfile << "v " << v_3.x << " " << v_3.y << " " << v_3.z << std::endl;
		outfile << "v " << v_4.x << " " << v_4.y << " " << v_4.z << std::endl;
		outfile << "v " << v_5.x << " " << v_5.y << " " << v_5.z << std::endl;
		outfile << "v " << v_6.x << " " << v_6.y << " " << v_6.z << std::endl;
		outfile << "v " << v_7.x << " " << v_7.y << " " << v_7.z << std::endl;
		outfile << "v " << v_8.x << " " << v_8.y << " " << v_8.z << std::endl;

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
	a = normalize(a);
	b = normalize(b);
	vec3 c = normalize(cross(a, b));

	// non-orthogonal base
	//glm::mat3 M_target(a, b, c);

	// orthogonal base
	vec3 n = normalize(cross(b, c));
	glm::mat3 M_target(b, c, n);

	glm::mat3 M_trafo = inverse(M_target);
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
	return (1 << subd_level) + 1; // 2^subd_level + 1
}

#ifdef PROJECTION
glm::vec3 subd_subpatch::oriented_to_projected(const glm::vec3 &p) const {
	return project(p, proj);
}

glm::vec3 subd_subpatch::projected_to_oriented(const glm::vec3 &p) const {
	return project(p, inverse(proj));
}
#endif

void subd_subpatch::build_bvh(const subd_patch *parent, bool debug) {
	glm::mat3 &T = trafo;
	glm::mat3 T_inv = inverse(trafo);
	const auto &verts = parent->verts;
	uint32_t size = (len()-1)*(len()-1);
	
	int nodes_count = geometric_series4(subd_level-1);
	nodes.resize(nodes_count);

#if defined(SLAB_COMPRESSION)
	patch_node basic_nodes[nodes_count];
#else
	std::vector<patch_node> &basic_nodes = nodes;
#endif

#ifdef PROJECTION
	// Scale projection so that the root box bounds will result in (-1,y_min,-1)x(1,y_max,1)
	aabb subpatch_bounds;
	for (uint32_t morton = 0; morton < size; ++morton) {
		uint32_t x = decode_morton(morton);
		uint32_t y = decode_morton(morton >> 1);
		uint32_t vert_index = vert_start + y*parent->len()+x;
		
		// TODO: Can be built more efficiently, currently vertices are likely added multiple times adding using neighbouring quads
		subpatch_bounds.grow(project(T * verts[vert_index].pos, proj));
		subpatch_bounds.grow(project(T * verts[parent->vert_right(vert_index)].pos, proj));
		subpatch_bounds.grow(project(T * verts[parent->vert_down(vert_index)].pos, proj));
		subpatch_bounds.grow(project(T * verts[parent->vert_down_right(vert_index)].pos, proj));
	}
	std::vector<glm::vec2> input;
	input.emplace_back(subpatch_bounds.min.x, subpatch_bounds.min.z);
	input.emplace_back(subpatch_bounds.min.x, subpatch_bounds.max.z);
	input.emplace_back(subpatch_bounds.max.x, subpatch_bounds.min.z);
	input.emplace_back(subpatch_bounds.max.x, subpatch_bounds.max.z);
	std::vector<glm::vec2> target;
	target.emplace_back(-1.f, -1.f);
	target.emplace_back(-1.f, 1.f);
	target.emplace_back(1.f, -1.f);
	target.emplace_back(1.f, 1.f);
	proj = compute_homography(input, target) * proj;
	//subpatch_bounds.min.x = -1.f, subpatch_bounds.min.z = -1.f;
	//subpatch_bounds.max.x = 1.f, subpatch_bounds.max.z = 1.f;
#endif

	// Build leaf BVH level
	int off_children = geometric_series4(subd_level-2);
	for (uint32_t morton = 0; morton < size; ++morton) {
		uint32_t x = decode_morton(morton);
		uint32_t y = decode_morton(morton >> 1);
		uint32_t vert_index = vert_start + y*parent->len()+x;

		//std::cout << "Trafo (" << morton << "):\t" << glm::to_string(T_inv) << std::endl;
		
		// TODO: Can be built more efficiently, currently vertices are likely added multiple times adding using neighbouring quads
		aabb box;
#ifndef PROJECTION
		box.grow(T * verts[vert_index].pos);
		box.grow(T * verts[parent->vert_right(vert_index)].pos);
		box.grow(T * verts[parent->vert_down(vert_index)].pos);
		box.grow(T * verts[parent->vert_down_right(vert_index)].pos);
#else
		box.grow(project(T * verts[vert_index].pos, proj));
		box.grow(project(T * verts[parent->vert_right(vert_index)].pos, proj));
		box.grow(project(T * verts[parent->vert_down(vert_index)].pos, proj));
		box.grow(project(T * verts[parent->vert_down_right(vert_index)].pos, proj));
#endif

		if (subd_level > 0) {
			basic_nodes[off_children+(morton>>2)].boxes[morton & 3] = box;
		}
		else {
			root_box = box;
			return; 
		}
	}

	// Build inner BVH levels
	int off = 0;
	for (int i = 1; i <= subd_level; i++) {
		int len = 1 << (subd_level-i); // 2^(subd_level-i);
		size = len*len;
		if (i > 1)			off_children = off;
		if (i < subd_level)	off = geometric_series4(subd_level-i-2);

		for (uint32_t j = 0; j < size; ++j) {
			const auto &child_node = basic_nodes[off_children + j];
			aabb box;
			box.grow(child_node.boxes[0]);
			box.grow(child_node.boxes[1]);
			box.grow(child_node.boxes[2]);
			box.grow(child_node.boxes[3]);

			if (i < subd_level) {
				basic_nodes[off+(j>>2)].boxes[j & 3] = box;
			}
			else {
				root_box = box;
			}
		}
	}

#if defined(SLAB_COMPRESSION)
	off = geometric_series4(subd_level-2);
	uint32_t off_parent = 0;
	for (int i = 1; i <= subd_level; i++) {
		int len = 1 << (subd_level-i); // 2^(subd_level-i);
		size = len*len;
		if (i > 1)			off = off_parent;
		if (i < subd_level)	off_parent = geometric_series4(subd_level-i-2);

		for (uint32_t j = 0; j < size; ++j) {
			auto &node = nodes[off + j];
			const auto &basic_node = basic_nodes[off + j];
			const aabb &parent_box = (i < subd_level) ?
									   basic_nodes[off_parent+(j>>2)].boxes[j & 3]
									 : root_box;

			node = patch_slab_node::from(basic_node);
			//node = patch_slab_node::from(basic_node, parent_box);
	/*#ifndef QUANTIZATION
			node = patch_slab_node::from(basic_node);
	#else
			node = patch_slab_node::from(basic_node, parent_box);
	#endif*/

		}
	}
#endif

	/*assert(root_box.min.x == slab_nodes[0].x_slabs[0]);
	assert(root_box.max.x == slab_nodes[0].x_slabs[3]);
	assert(root_box.min.z == slab_nodes[0].z_slabs[0]);
	assert(root_box.max.z == slab_nodes[0].z_slabs[3]);*/
	//assert(root_box.min.y == slab_nodes[0].y);

/*#ifdef SLAB_COMPRESSION
	// Convert to compressed slab nodes
	for (uint32_t i = 0; i < nodes_count; i++) {
		slab_nodes[i] = patch_slab_node::from(nodes[i]);
	}
#endif*/
}

void subd_patch::build_bvh(int32_t align_level, bool debug) {
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

	// REVIEW: Check the configuration of these fields
	this->align_boxes = align_level >= 0;
	if (align_level > subd_level)
		align_level = subd_level;
	this->align_level = align_level;

	uint32_t blocks = 1 << 2 * align_level;				// 4^align_level
	if (align_boxes)
		subpatches.resize(blocks);

	int nodes_count = align_boxes ?
						geometric_series4(align_level-1)
					  : geometric_series4(subd_level-1);
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

			// base vertices
			const vec3 &a = verts[vert_index].pos;
			const vec3 &b = verts[vert_down(vert_index, block_len)].pos;
			const vec3 &c = verts[vert_down_right(vert_index, block_len)].pos;
			const vec3 &d = verts[vert_right(vert_index, block_len)].pos;

			// base from one corner
			//glm::mat3 T = trafo_matrix(b-a, d-a);

			// base from averaged diagonales
			//glm::mat3 T = trafo_matrix(b-d, c-a);

			// base from averaged opposite sides
			glm::mat3 T = trafo_matrix((b-a) + (c-d), (d-a) + (c-b));

#ifdef PROJECTION
			// Calculate projection matrix
			std::vector<glm::vec2> input;
			input.emplace_back(xz(T * a));
			input.emplace_back(xz(T * b));
			input.emplace_back(xz(T * d));
			input.emplace_back(xz(T * c));
			/*input.emplace_back(1, 1);
			input.emplace_back(4, 1);
			input.emplace_back(2, 3);
			input.emplace_back(5, 4);*/
			std::vector<glm::vec2> target;
			target.emplace_back(-1.f, -1.f);
			target.emplace_back(-1.f, 1.f);
			target.emplace_back(1.f, -1.f);
			target.emplace_back(1.f, 1.f);
			glm::mat3 proj = compute_homography(input, target);
			//glm::mat3 proj(1.f);

			/*std::cout << "result: " << project(vec3(5,1.3,4), proj) << std::endl;
			std::cout << "result: " << project(vec3(4,1.3,1), proj) << std::endl;
			std::cout << "result: " << project(vec3(4,0.4,1), proj) << std::endl;
			std::cout << "result: " << project(vec3(1.5,0.4,2), proj) << std::endl;
			std::cout << "result: " << project(vec3(1.5,0.6,2), proj) << std::endl;

			std::cout << "result: " << project(vec3(2,0.6,1), proj) << std::endl;
			std::cout << "result: " << project(vec3(3,0.6,1), proj) << std::endl;
			std::cout << "result: " << project(vec3(2.5,0.6,1), proj) << std::endl;
			std::cout << "result: " << project(project(vec3(2.5,0.6,1), proj), inverse(proj)) << std::endl;*/
#endif

			// Init subpatch
			subd_subpatch &sub = subpatches[morton];
			sub.vert_start = vert_index;
			sub.trafo = T;
#ifdef PROJECTION
			sub.proj = proj;
#endif
			sub.subd_level = aligned_subd_level;
		}

		for (uint32_t morton = 0; morton < blocks; morton++)
			subpatches[morton].build_bvh(this, debug);

	}


	/* Create first level of TL-BVH */
	int off_children = geometric_series4(align_level-2);
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

#ifndef PROJECTION
			box.grow(sub.root_box, T_inv);
#else
			box.grow(sub.root_box, T_inv, inverse(sub.proj));
#endif
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

		if (align_level > 0)	nodes[off_children+(morton>>2)].boxes[morton%4] = box;
		else					root_box = box;
		// TODO/TMP: this is only required for passing to GPU -> delete and calculate box index from parent where needed
		if (align_boxes)		subpatches[morton].root_box_world = box;

	}


	/* Create upper levels of the TL-BVH */
	int off = 0;
	for (int i = 1; i <= align_level; i++) {
		int len = 1 << (align_level-i); // 2^(align_level-i);
		uint32_t size = len*len;
		if (i > 1)			off_children = off;
		if (i < align_level)	off = geometric_series4(align_level-i-2);

		for (uint32_t j = 0; j < size; ++j) {
			const patch_node &child_node = nodes[off_children + j];
			aabb box;
			box.grow(child_node.boxes[0]);
			box.grow(child_node.boxes[1]);
			box.grow(child_node.boxes[2]);
			box.grow(child_node.boxes[3]);

			if (i < align_level)	nodes[off+(j>>2)].boxes[j%4] = box;
			else					root_box = box;
		}
	}
}

void subd_patch::export_bvh(const std::string &path) const {
	bvh_writer writer(path, "S" + std::to_string(subd_level) + "_A" + std::to_string(align_level));
	// Init writer
	writer.name_ext = "_aabb";
	writer.start_bvh();
	writer.set_trafo(glm::mat3(1.f));
#ifdef PROJECTION
	writer.set_proj(glm::mat3(1.f));
#endif

	// Start TL-BVH
	writer.print_box(root_box);
	for (int32_t level = 0; level < align_level; level++) {
		writer.new_level();
		uint32_t child_node_base = geometric_series4(level-1);
		uint32_t size = 1 << 2*level; // 4^level
		for (uint32_t morton = 0; morton < size; morton++) {
			const patch_node &node = nodes[child_node_base + morton];
			for (const auto &box : node.boxes)
				writer.print_box(box);

		}
	}

	// Start BL-BVH
	bool align_boxes = subpatches.size() > 0;
	if (!align_boxes)
		return;
	
	uint32_t aligned_subd_level = subpatches[0].subd_level;
	writer.name_ext = "_aligned";
	writer.new_object();

	for (const auto &sub : subpatches) {
		writer.set_trafo(inverse(sub.trafo));
#ifdef PROJECTION
		writer.set_proj(inverse(sub.proj));
#endif
		writer.print_box(sub.root_box);
	}

	for (int32_t level = 0; level < aligned_subd_level; level++) {
		writer.new_level();
		uint32_t child_node_base = geometric_series4(level-1);
		uint32_t size = 1 << 2*level; // 4^level
		for (const auto &sub : subpatches) {
			writer.set_trafo(inverse(sub.trafo));
#ifdef PROJECTION
			writer.set_proj(inverse(sub.proj));
#endif
			for (uint32_t morton = 0; morton < size; morton++) {
#ifndef SLAB_COMPRESSION
				const patch_node &node = sub.nodes[child_node_base + morton];
				for (const auto &box : node.boxes)
					writer.print_box(box);
#else
				const patch_slab_node &node = sub.nodes[child_node_base + morton];
				for (uint32_t i = 0; i < 4; i++) {
	#ifndef HALF_SLAB_COMPRESSION
					writer.print_box(node.get_box(i));
	#else
					//writer.print_box(node.get_box(i));
					writer.print_box(sub.box_from_node(child_node_base + morton, i));
	#endif
				}
#endif
			}
		}
	}

}

std::array<triangle, 2> subd_patch::tris(int vert_quad_id) const {
	return {
		tri(vert_quad_id, true),
		tri(vert_quad_id, false)
	};
}

triangle subd_patch::tri(int vert_quad_id, bool upper) const {
	triangle tri;
	tri.material_id = material_id;
	if (upper) {
		tri.a = vert_quad_id;
		tri.b = vert_down(vert_quad_id);
		tri.c = vert_right(vert_quad_id);
	}
	else {
		tri.a = vert_down_right(vert_quad_id);
		tri.b = vert_right(vert_quad_id);
		tri.c = vert_down(vert_quad_id);
	}

	return tri;
}

std::tuple<uint32_t, uint32_t> subd_patch::xy_from_index(uint32_t index) const {
	uint32_t x = decode_morton(index);
	uint32_t y = decode_morton(index >> 1);
	return {x, y};
}

uint32_t subd_patch::quad_ref_from_index(uint32_t index, uint32_t level) const {
	auto [x, y] = xy_from_index(index);
	return y*len(level) + x;
}

uint32_t subd_patch::quad_ref_from_index(uint32_t index) const {
	return quad_ref_from_index(index, subd_level);
}

uint32_t subd_patch::index_from_quad_ref(uint32_t vert_quad_id) const {
	uint32_t x = vert_quad_id % len(); //TODO/REVIEW: switch / and % to shift operations
	uint32_t y = vert_quad_id / len();
	return encode_morton(x, y);
}

const subd_subpatch &subd_patch::subpatch_from_index(uint32_t index) const {
	assert(subpatches.size() > 0);
	uint32_t aligned_subd_level = subpatches[0].subd_level;
	uint32_t subpatch_id = index >> 2*aligned_subd_level; // divide by subpatch size (#quads in subpatch)
	return subpatches[subpatch_id];
}

#ifndef SLAB_COMPRESSION
const aabb &subd_subpatch::box_from_index(uint32_t index) const {
	if (subd_level == 0) return root_box;

	uint32_t modulo_mask = ~(0xFFFFFFFF << 2*subd_level);
	uint32_t quad_ref_local = index & modulo_mask;
	uint32_t node_index = (quad_ref_local >> 2) + geometric_series4(subd_level-2);
	uint32_t box_index = quad_ref_local & 0x3;
	return nodes[node_index].boxes[box_index];
}
#else
aabb subd_subpatch::box_from_index(uint32_t index) const {
	if (subd_level == 0) return root_box;

	uint32_t modulo_mask = ~(0xFFFFFFFF << 2*subd_level);
	uint32_t quad_ref_local = index & modulo_mask;
	uint32_t node_index = (quad_ref_local >> 2) + geometric_series4(subd_level-2);
	uint32_t box_index = quad_ref_local & 0x3;
#ifndef HALF_SLAB_COMPRESSION
	return nodes[node_index].get_box(box_index);
#else
	return box_from_node(node_index, box_index);
#endif
}
#endif
#if defined(SLAB_COMPRESSION) && defined(HALF_SLAB_COMPRESSION)
void assert_box_compare(const aabb &a, const aabb &b) {
	assert(a.min.y == b.min.y);
	assert(a.max.y == b.max.y);

	assert(a.min.x == b.min.x);
	assert(a.min.z == b.min.z);
	assert(a.max.x == b.max.x);
	assert(a.max.z == b.max.z);
}

void debug_boxes(const aabb &a, const aabb &b, int32_t id = -1) {
	if (id > -1) std::cout << "Check " << id << ":\n";
	std::cout << "Box: [(" << a.min << "), (" << a.max << ")]\n";
	std::cout << "Ref: [(" << b.min << "), (" << b.max << ")]\n";
	assert_box_compare(a, b);
}

float subd_subpatch::slab_from_parent(uint32_t node_index, uint32_t child_index, bool is_x_slab, uint32_t slab_pos) const {
	// [FEAT-PROJ] TODO !
	/*#ifdef PROJECTION
				float3 root_min = make_float3(-1.f, root_min_y, -1.f);
				float3 root_max = make_float3(1.f, root_max_y, 1.f);
	#endif*/
	if (node_index == 0) {
		// case no parent: take missing values from root box
		if (is_x_slab) {
			if (slab_pos == 0)  return root_box.max.x; // right
			else				return root_box.min.x; // left
		}
		if (!is_x_slab) {
			if (slab_pos == 0)  return root_box.max.z; // bottom
			else				return root_box.min.z; // top
		}
	}

	const patch_slab_node &node = nodes[node_index];

	// Calculate parent node index
	uint32_t trav_level = log4_clz(1+3*node_index);
	uint32_t off_current_level = geometric_series4(trav_level-1);
	uint32_t off_parent_level = geometric_series4(trav_level-2);
	uint32_t idx_current_relative = node_index - off_current_level;
	uint32_t idx_parent_relative = idx_current_relative >> 2; //(/ 4)

	uint32_t parent_index = off_parent_level + idx_parent_relative;
	uint32_t parents_child_index = idx_current_relative & 3; // equals modulo 4 for non-negative values

	bool delegate_to_parent = parents_child_index == child_index;
	if (is_x_slab && slab_pos == 0) {
		// right
		delegate_to_parent = delegate_to_parent || (parents_child_index == 1 && child_index == 3);
		delegate_to_parent = delegate_to_parent || (parents_child_index == 3 && child_index == 1);
	}
	else if (is_x_slab && slab_pos == 1) {
		// left
		delegate_to_parent = delegate_to_parent || (parents_child_index == 0 && child_index == 2);
		delegate_to_parent = delegate_to_parent || (parents_child_index == 2 && child_index == 0);
	}
	else if (!is_x_slab && slab_pos == 0) {
		// bottom
		delegate_to_parent = delegate_to_parent || (parents_child_index == 2 && child_index == 3);
		delegate_to_parent = delegate_to_parent || (parents_child_index == 3 && child_index == 2);
	}
	else if (!is_x_slab && slab_pos == 1) {
		// top
		delegate_to_parent = delegate_to_parent || (parents_child_index == 0 && child_index == 1);
		delegate_to_parent = delegate_to_parent || (parents_child_index == 1 && child_index == 0);
	}
	if (delegate_to_parent)
		return slab_from_parent(parent_index, parents_child_index, is_x_slab, slab_pos);

	const patch_slab_node &parent = nodes[parent_index];
	return is_x_slab ? parent.x_slabs[slab_pos] : parent.z_slabs[slab_pos];
}

aabb subd_subpatch::box_from_node(uint32_t node_index, uint32_t box_index, bool debug) const {
	aabb box;
	const patch_slab_node &node = nodes[node_index];
	//aabb dbg_box = node.get_box(box_index);

	if (box_index == 0)      { box.max.x = node.x_slabs[0]; box.max.z = node.z_slabs[0]; box.min.y = node.y_min[0]; box.max.y = node.y_max[0]; }
	else if (box_index == 1) { box.min.x = node.x_slabs[1]; box.max.z = node.z_slabs[0]; box.min.y = node.y_min[1]; box.max.y = node.y_max[1]; }
	else if (box_index == 2) { box.max.x = node.x_slabs[0]; box.min.z = node.z_slabs[1]; box.min.y = node.y_min[2]; box.max.y = node.y_max[2]; }
	else if (box_index == 3) { box.min.x = node.x_slabs[1]; box.min.z = node.z_slabs[1]; box.min.y = node.y_min[3]; box.max.y = node.y_max[3]; }

	if (box_index == 0) {
		box.min.x = slab_from_parent(node_index, box_index, true, 1);	// left
		box.min.z = slab_from_parent(node_index, box_index, false, 1);	// top
	}
	else if (box_index == 1) {
		box.max.x = slab_from_parent(node_index, box_index, true, 0);	// right
		box.min.z = slab_from_parent(node_index, box_index, false, 1);	// top
	}
	else if (box_index == 2) {
		box.min.x = slab_from_parent(node_index, box_index, true, 1);	// left
		box.max.z = slab_from_parent(node_index, box_index, false, 0);	// bottom
	}
	else if (box_index == 3) {
		box.max.x = slab_from_parent(node_index, box_index, true, 0);	// right
		box.max.z = slab_from_parent(node_index, box_index, false, 0);	// bottom
	}

	return box;
}
#endif

#ifdef BOX_APPROXIMATION
void subd_patch::prepare_box_approximation() {
	// Store patch corner vertex data
	uint32_t step = len() - 1;
	const vertex *corners[4];
	corners[0] = &verts[0];
	corners[1] = &verts[vert_right(0, step)];
	corners[2] = &verts[vert_down(0, step)];
	corners[3] = &verts[vert_down_right(0, step)];
	for (uint32_t i = 0; i < 4; ++i) {
		data[i].tc = corners[i]->tc;
		data[i].norm = corners[i]->norm;
	}

	// Remove actual geometry data in box approximation
	//patch.verts.clear();
	//patch.verts.shrink_to_fit();

	#ifndef KEEP_GEOMETRY
	// Clear and deallocate vertex memory
	// reference: https://cplusplus.com/reference/vector/vector/clear/
	std::vector<vertex>().swap(verts);
	#endif
}

std::tuple<float, float> subd_patch::global_uvs(quad_ref quad_ref, float local_u, float local_v) const {
	uint32_t quad_len = len() - 1;
	auto [x, y] = xy_from_index(quad_ref.ref());
	float global_u = x * 1.f/quad_len;
	float global_v = y * 1.f/quad_len;

	float step = 1.f / quad_len;
	if (quad_ref.is_upper_tri()) {
		global_u += step * local_u;
		global_v += step * local_v;
	}
	else {
		global_u += step * (1.f - local_u);
		global_v += step * (1.f - local_v);
	}

	return {global_u, global_v};
}
#endif
