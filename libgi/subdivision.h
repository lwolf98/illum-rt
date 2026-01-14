#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>
#include <assimp/mesh.h>
#include <pxr/pxr.h>
#include "driver/defines.h"
#include "rt.h"
#include "intersect.h"
#include "subdivision_nodes.h"

PXR_NAMESPACE_OPEN_SCOPE
// forward declaration to avoid full include of pxr/usd/usdGeom/mesh.h
class UsdGeomMesh;
PXR_NAMESPACE_CLOSE_SCOPE

namespace subd {
	/* Basic operations */
	static uint32_t log2_clz(uint32_t x) {
		return 31 - __builtin_clz(x);
	}

	static uint32_t log4_clz(uint32_t x) {
		return (31 - __builtin_clz(x)) >> 1;
	}

	static int geometric_series4(int iterations) {
		return (1 - (1 << ((iterations+1)<<1))) / (-3);
	}

	static uint32_t child_node_base(
		uint32_t trav_level,
		uint32_t index
	) {
		uint32_t off_current_level = geometric_series4(trav_level-1);
		uint32_t off_child_level = geometric_series4(trav_level);
		uint32_t idx_current_relative = index - off_current_level;
		uint32_t idx_child_relative = idx_current_relative << 2; //(* 4)
		uint32_t index_child = off_child_level + idx_child_relative;
		return index_child;
	}

	static uint32_t child_node_base(
			uint32_t index
		) {
			uint32_t trav_level = log4_clz(1+3*index);
			return child_node_base(trav_level, index);
	}

	/* Structures */
	struct edge {
		int v1, v2;
		float sharpness;
		std::vector<int> face_ids;

		edge() = default;
		edge(int v1, int v2, float sharpness) : v1(v1), v2(v2), sharpness(sharpness) {}
		edge(int v1, int v2) : edge(v1, v2, 0.f) {}
		edge(int v1, int v2, float sharpness, int face_id) : edge(v1, v2, sharpness) {
			face_ids.push_back(face_id);
		}

		bool face_exists(int id) const;
	};

	struct ctrl_vertex : ::vertex {
		std::vector<uint64_t> edge_ids;
		std::vector<int> face_ids;
		std::vector<std::pair<uint32_t, uint32_t>> patch_positions;

		ctrl_vertex() {}
		ctrl_vertex(glm::vec3 v) {
			pos = v;
		}

		bool edge_exists(uint64_t id) const;
		bool face_exists(int id) const;
	};

	class edge_list {
		std::map<uint64_t, edge> edges;
		std::map<uint64_t, int> edge_indices;
		bool initialized = false;

		uint64_t hash(int a, int b) const;

	public:
		uint64_t add(int a, int b);
		uint64_t add(int a, int b, float sharpness);
		uint64_t add(const edge &e);
		uint64_t get_key(int a, int b) const;
		uint64_t get_key(const edge &e) const;
		int get_index(uint64_t key) const;
		int get_index(int a, int b) const;
		int get_index(const edge &e) const;
		edge& get(uint64_t id);
		edge& get(int a, int b);
		edge& get_next(std::map<uint64_t, edge>::iterator &it);
		int size() const;
		bool exists(int a, int b) const;
		void clear();
		void finish_init();
		std::map<uint64_t, edge>::iterator iterator() {
			return edges.begin();
		}
	};

	struct vertex_config {
		uint pos;
		uint tc;
	};

	struct face {
		glm::vec3 normal;
		std::vector<vertex_config> verts;
		int32_t material_id;
		int32_t patch_id;
		uint32_t patch_x;
		uint32_t patch_y;

		face() : normal(0), material_id(-1), patch_id(-1), patch_x(0), patch_y(0) {}
		uint size() { return verts.size(); }
	};

	struct base_node {
		aabb box;
		uint32_t left, right;
		uint32_t triangle = (uint32_t)-1;
		//! is the node an inner node (as opposed to a leaf)
		bool inner() const { return triangle == (uint32_t)-1; }
		void set_secondary_value(uint32_t val) {
			triangle = ((uint32_t)-1) - (val + 1);
		}
		uint32_t get_secondary_value() const {
			return ((uint32_t)-1) - (triangle + 1);
		}
		bool is_subd_leaf() const {
			return left >= (uint32_t)-2 && right >= (uint32_t)-2;
		}
		bool is_subd_root_and_leaf() const {
			return left == (uint32_t)-2 && right == (uint32_t)-2;
		}
		bool is_only_subd_root() const {
			return !inner() && !is_subd_leaf();
		}
	};

	struct subd_patch;
	struct subd_subpatch {
#if defined(SLAB_COMPRESSION) || defined(QUANTIZATION)
		std::vector<patch_slab_node> nodes;
#else
		std::vector<patch_base_node> nodes; //REVIEW: BASE?
#endif
		uint32_t vert_start;
		glm::mat3 trafo;
#ifdef PROJECTION
		glm::mat3 proj;
#endif
		aabb root_box;
		aabb root_box_world; // TODO/TMP: this is only required for passing to GPU -> delete and calculate box index from parent where needed
		uint32_t subd_level;

		void build_bvh(const subd_patch *parent, bool debug = false);
		uint32_t len() const;
#ifndef SLAB_COMPRESSION
		const aabb &box_from_index(uint32_t local_index) const;
#else
		aabb box_from_index(uint32_t local_index) const;
	#ifdef HALF_SLAB_COMPRESSION
		aabb box_from_node(uint32_t node_index, uint32_t box_index, bool debug = false) const;
	private:
		float slab_from_parent(uint32_t node_index, uint32_t child_index, bool is_x_slab, uint32_t slab_pos) const;
	public:
	#endif
#endif


#ifdef PROJECTION
		glm::vec3 oriented_to_projected(const glm::vec3 &p) const;
		glm::vec3 projected_to_oriented(const glm::vec3 &p) const;
#endif
	};

#ifdef BOX_APPROXIMATION
	struct patch_vertex {
		glm::vec2 tc;
		glm::vec3 norm;
	};
#endif

	struct subd_patch {
		std::vector<vertex> verts;
		std::vector<patch_base_node> nodes;
		std::vector<subd_subpatch> subpatches;
		aabb root_box;
		uint32_t material_id;
		uint32_t subd_level;
		int32_t align_level;
		bool align_boxes;
#ifdef BOX_APPROXIMATION
		patch_vertex data[4];
#endif

		subd_patch(uint32_t level) : subd_patch(level, 0) {}
		subd_patch(uint32_t level, uint32_t material_id) : subd_level(level),
														   material_id(material_id) {
			uint32_t size = len()*len();
			//TODO: maybe use resize with init value to avoid own init loop
			verts.reserve(size);
			for (uint32_t i = 0; i < size; ++i) {
				vertex v;
				v.pos = vec3(0);
				verts.emplace_back(v);
			}
		}

		uint32_t len() const;
		uint32_t len(uint32_t level) const;
		uint32_t vert_right(uint32_t vert_id, uint32_t step = 1) const;
		uint32_t vert_down(uint32_t vert_id, uint32_t step = 1) const;
		uint32_t vert_down_right(uint32_t vert_id, uint32_t step = 1) const;
		uint32_t vert_offset(uint32_t vert_id, int32_t off_x, int32_t off_y) const;
		void build_bvh(int32_t align_level, bool debug = false);
		std::array<triangle, 2> tris(int vert_quad_id) const;
		triangle tri(int vert_quad_id, bool upper) const;

		// Index operations
		std::tuple<uint32_t, uint32_t> xy_from_index(uint32_t index) const;
		uint32_t quad_ref_from_index(uint32_t index, uint32_t level) const;
		uint32_t quad_ref_from_index(uint32_t index) const;
		//uint32_t subpatch_ref_from_index(uint32_t index) const;
		uint32_t index_from_quad_ref(uint32_t vert_quad_id) const;

		const subd_subpatch &subpatch_from_index(uint32_t index) const;
		//const aabb &box_from_index(const subd_subpatch &subpatch, uint32_t index) const;

		void print_verts() const;
		void print_vert_tcs() const;
		void export_bvh(const std::string &path) const;

#ifdef BOX_APPROXIMATION
		void prepare_box_approximation();
		std::tuple<float, float> global_uvs(quad_ref quad_ref, float local_u, float loacl_v) const;
#endif

		//private:
		//int calculate_morton_code(int x, int y) const;
		//tuple<int, int> evaluate_morton_code(int morton_code) const;
	};

	typedef std::function<glm::vec4(glm::vec2)> sample_tex;
	struct mesh {
		bool has_normals;
		bool has_texture;
		std::vector<ctrl_vertex> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> tex_coords;
		std::vector<face> faces;
		edge_list edges;
		edge_list creases;
		std::vector<subd_patch> patches;
		bool storage_type_patches;

		int get_vert_id(glm::vec3 v_pos);
		void update(bool clear = false);
		void subdivide(uint32_t level);
		void triangulate();
		void displace(sample_tex sample, float strength);
		void build_patch_bvhs(uint32_t align_level) {
			if (storage_type_patches) {
				#pragma omp parallel
				{
					#pragma omp single
					{
						for (int i = 0; i < patches.size(); i++) {
							auto &patch = patches[i];
							patch.build_bvh(align_level, true);
						}
					}
				}
			}
		}
#ifdef BOX_APPROXIMATION
		void prepare_box_approximation();
#endif

		mesh() : storage_type_patches(true) { }

	private:
		void subdivide_internal(uint32_t end_level);
		void update_topology();
		void pass_tcs();
		uint64_t add_edge(int a, int b, int f_id);
		void update_vertex(ctrl_vertex &v, int f_id, uint64_t e_id);

		glm::vec3 calc_smooth_edge_vertex(const edge &e, const std::vector<glm::vec3> &face_vertices);
		glm::vec3 calc_sharp_edge_vertex(const edge &e);
		glm::vec3 calc_vertex_vertex(const ctrl_vertex &v, const std::vector<glm::vec3> &edge_vertices, const std::vector<glm::vec3> &face_vertices);

		void calculate_face_normals();
		void calculate_vertex_normals();
	};

	struct object {
		std::string name;
		std::string material;

		struct mesh mesh;

		object() : name(""), material("") {}
		object(aiMesh *mesh_ai, bool subd_type_patches = true, std::string mat_name = "") : object() {
			material = mat_name;
			mesh.storage_type_patches = subd_type_patches;
			init_object(mesh_ai);
		}
		object(const pxr::UsdGeomMesh &usd_mesh, bool subd_type_patches = true, std::string mat_name = "") : object() {
			material = mat_name;
			mesh.storage_type_patches = subd_type_patches;
			init_object(usd_mesh);
		}
		bool has_material() { return material != ""; }
		void write_obj(std::string outfile_name, bool write_normals, std::string mtllib_path = "", float n_len = 1.f);
		void write_obj(bool write_normals, std::string mtllib_path = "", float n_len = 1.f) {
			write_obj("out_" + name + ".obj", write_normals, mtllib_path, n_len);
		}

	private:
		void init_object(aiMesh *mesh_ai);
		void init_object(const pxr::UsdGeomMesh &usd_mesh);

	};
}