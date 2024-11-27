#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <assimp/mesh.h>
#include <tinyusdz.hh>
#include <pxr/usd/usdGeom/mesh.h>
#include "subdivision.h"
#include "rt.h"

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
		uint size() { return verts.size(); }
	};

	struct mesh {
		bool has_normals;
		bool has_texture;
		std::vector<ctrl_vertex> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> tex_coords;
		std::vector<face> faces;
		edge_list creases;
		int get_vert_id(glm::vec3 v_pos);
		void update();
		void subdivide();
		void calculate_vertex_normals();
		void triangulate();

	private:
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