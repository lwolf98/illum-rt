#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "rt.h"

namespace subd {
	struct edge;
	struct ctrl_vertex;
	class edge_list;
	//void subdivide(std::vector<ctrl_vertex> &vertices, std::vector<std::vector<uint32_t>> &faces);
	void subdivide(std::vector<ctrl_vertex> &vertices, std::vector<std::vector<int>> &faces, std::vector<glm::vec3> &normals);
	void triangulate(const std::vector<ctrl_vertex> &vertices, std::vector<std::vector<int>> &faces, std::vector<glm::vec3> &normals);
	void write_obj(const std::vector<ctrl_vertex> &vertices, const std::vector<std::vector<int>> &faces, const std::vector<glm::vec3> &normals, const std::string name);

	struct edge {
		int v1, v2;
		std::vector<int> face_ids;

		edge(int v1, int v2) : v1(v1), v2(v2) {}

		bool face_exists(int id) const;
	};

	struct ctrl_vertex : ::vertex {
		//glm::vec3 v;
		//glm::vec3 n;
		//glm::vec2 tc;
		std::vector<int> edge_ids;
		std::vector<int> face_ids;

		ctrl_vertex() {}
		ctrl_vertex(glm::vec3 v) {
			pos = v;
		}
		//ctrl_vertex(vec3_t v) : v(glm::vec3(v.x[0], v.x[1], v.x[2])) {}

		bool edge_exists(int id) const;
		bool face_exists(int id) const;
	};

	class edge_list {
		std::vector<edge> edges;

	public:
		int add(int a, int b);
		int get_id(int a, int b) const;
		edge& get(int id);
		int size() const;
		bool exists(int a, int b) const;
		void clear();
	};
}