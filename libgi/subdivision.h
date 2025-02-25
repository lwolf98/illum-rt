#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <assimp/mesh.h>
#include <tinyusdz.hh>
#include <pxr/usd/usdGeom/mesh.h>
#include "rt.h"
#include "intersect.h"

namespace subd {
	struct edge {
		int v1, v2;
		float sharpness;
		std::vector<int> face_ids;

		edge(int v1, int v2, float sharpness) : v1(v1), v2(v2), sharpness(sharpness) {}
		edge(int v1, int v2) : edge(v1, v2, 0.f) {}

		bool face_exists(int id) const;
	};

	struct ctrl_vertex : ::vertex {
		std::vector<int> edge_ids;
		std::vector<int> face_ids;

		ctrl_vertex() {}
		ctrl_vertex(glm::vec3 v) {
			pos = v;
		}

		bool edge_exists(int id) const;
		bool face_exists(int id) const;
	};

	class edge_list {
		std::vector<edge> edges;

	public:
		int add(int a, int b);
		int add(int a, int b, float sharpness);
		int get_id(int a, int b) const;
		edge& get(int id);
		int size() const;
		bool exists(int a, int b) const;
		void clear();
	};

	struct vertex_config {
		uint pos;
		uint tc;
	};

	struct face {
		glm::vec3 normal;
		std::vector<vertex_config> verts;
		uint32_t patch_id;
		uint32_t patch_x;
		uint32_t patch_y;

		face() : normal(0), patch_id(0), patch_x(0), patch_y(0) {}
		uint size() { return verts.size(); }
	};

	struct node {
		aabb box;
		uint32_t left = (uint32_t)-1;
		uint32_t right = (uint32_t)-1;
		uint32_t triangle = (uint32_t)-1;
		//! is the node an inner node (as opposed to a leaf)
		bool inner() const { return triangle == (uint32_t)-1; }
		void set_secondary_value(uint32_t val) {
			triangle = ((uint32_t)-1) - (val + 1);
		}
		uint32_t get_secondary_value() {
			return ((uint32_t)-1) - (triangle + 1);
		}
	};

	struct subd_patch {
		//std::vector<glm::vec3> verts;
		std::vector<vertex> verts;
		uint32_t bvh_node;
		std::vector<node> nodes;
		uint32_t material_id;
		uint32_t subd_level;

		subd_patch(uint32_t level) : subd_level(level) {
			uint32_t size = len()*len();
			verts.reserve(size);
			for (uint32_t i = 0; i < size; ++i) {
				vertex v;
				v.pos = vec3(0);
				verts.emplace_back(v);
			}
		}

		void print_verts();
		uint32_t len();
		uint32_t len(uint32_t level);
		uint32_t vert_right(uint32_t vert_id);
		uint32_t vert_down(uint32_t vert_id);
		uint32_t vert_down_right(uint32_t vert_id);
		uint32_t vert_offset(uint32_t vert_id, int32_t off_x, int32_t off_y);
		void build_bvh();
		int get_subd_quad(int morton_code);
		std::array<triangle, 2> tris(int morton_code);

		private:
		int calculate_morton_code(int x, int y);
		tuple<int, int> evaluate_morton_code(int morton_code);
	};

	struct mesh {
		bool has_normals;
		bool has_texture;
		std::vector<ctrl_vertex> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> tex_coords;
		std::vector<face> faces;
		edge_list creases;
		std::vector<subd_patch> patches;
		int get_vert_id(glm::vec3 v_pos);
		void update();
		void subdivide(uint32_t level);
		void calculate_vertex_normals();
		void triangulate();

	private:
		void subdivide_internal(uint32_t level);
		int add_edge(edge_list &edges, int a, int b, int f_id);
		void update_vertex(ctrl_vertex &v, int f_id, int e_id);

		glm::vec3 calc_smooth_edge_vertex(const edge &e, const std::vector<glm::vec3> &face_vertices);
		glm::vec3 calc_sharp_edge_vertex(const edge &e);
		glm::vec3 calc_vertex_vertex(const ctrl_vertex &v, edge_list &edges, const std::vector<glm::vec3> &edge_vertices, const std::vector<glm::vec3> &face_vertices);

		void calculate_face_normals();
	};

	struct object {
		std::string name;
		std::string material;

		struct mesh mesh;

		object() : name(""), material("") {}
		object(aiMesh *mesh_ai, std::string mat_name = "") : object() {
			material = mat_name;
			init_object(mesh_ai);
		}
		object(const tinyusdz::GeomMesh *usd_mesh, std::string mat_name = "") : object() {
			material = mat_name;
			init_object(usd_mesh);
		}
		object(const pxr::UsdGeomMesh &usd_mesh, std::string mat_name = "") : object() {
			material = mat_name;
			init_object(usd_mesh);
		}
		bool has_material() { return material != ""; }
		void write_obj(std::string outfile_name, bool write_normals, std::string mtllib_path = "");
		void write_obj(bool write_normals, std::string mtllib_path = "") {
			write_obj("out_" + name + ".obj", write_normals);
		}

	private:
		void init_object(aiMesh *mesh_ai);
		void init_object(const tinyusdz::GeomMesh *usd_mesh);
		void init_object(const pxr::UsdGeomMesh &usd_mesh);

	};
}