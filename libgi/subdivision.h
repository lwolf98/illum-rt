#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>
#include <assimp/mesh.h>
#include <pxr/pxr.h>
#include "rt.h"
#include "intersect.h"

PXR_NAMESPACE_OPEN_SCOPE
// forward declaration to avoid full include of pxr/usd/usdGeom/mesh.h
class UsdGeomMesh;
PXR_NAMESPACE_CLOSE_SCOPE

namespace subd {
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

	struct patch_node {
		aabb boxes[4];
	};

	struct subd_patch {
		std::vector<vertex> verts;
		std::vector<patch_node> nodes;
		aabb root_box;
		uint32_t material_id;
		uint32_t subd_level;

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

		void print_verts() const;
		void print_vert_tcs() const;
		uint32_t len() const;
		uint32_t len(uint32_t level) const;
		uint32_t vert_right(uint32_t vert_id) const;
		uint32_t vert_down(uint32_t vert_id) const;
		uint32_t vert_down_right(uint32_t vert_id) const;
		uint32_t vert_offset(uint32_t vert_id, int32_t off_x, int32_t off_y) const;
		void build_bvh();
		int get_subd_quad(int morton_code) const;
		std::array<triangle, 2> tris(int morton_code) const;
		triangle tri(int morton_code, bool upper) const;
		uint32_t quad_ref_from_index(uint32_t index) const;

		private:
		int calculate_morton_code(int x, int y) const;
		tuple<int, int> evaluate_morton_code(int morton_code) const;
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
		void update();
		void subdivide(uint32_t level);
		void triangulate();
		void displace(sample_tex sample, float strength);

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