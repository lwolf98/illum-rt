#include "objdraw.h"
#include <sstream>

namespace objdraw {
	/* icosphere: */
	icosphere::icosphere(vec3 pos = vec3(0), float scale = 1.0f) : pos(pos), scale(scale) { //, vertices(base_vertices) {
		//for (int i = 0; i < vertices.size(); i++)
		//	vertices[i] = vertices[i] + pos;
	}

	icosphere::icosphere(vec3 pos) : pos(pos), scale(1.0f) {}
	icosphere::icosphere() : pos(vec3(0)), scale(1.0f) {}

	string icosphere::obj_string(int32_t& start) {
		stringstream out;
		int32_t off = 0;

		for (auto v : vertices) {
			v = v * scale + pos;
			out << "v " << v.x << " " << v.y << " " << v.z << endl;
			off++;
		}

		out << endl;
		for (auto t : triangles) {
			out << "f " << t.a+start << " " << t.b+start << " " << t.c+start << endl;
		}
		start += off;

		return out.str();
	}

	/* path: */
	path::path(vec3 start_vertex) {
		push_vertex(start_vertex);
	}

	path::path() {}

	void path::push_vertex(vec3 v) {
		vertices.push_back(v);
	}

	string path::obj_string(int32_t& start) {
		stringstream out;
		int32_t off = 0;

		for (auto v : vertices) {
			out << "v " << v.x << " " << v.y << " " << v.z << endl;
			off++;
		}

		// i is vertex position starting from 1
		for (int i = 1; i < vertices.size(); ++i) {
			out << "l " << i+start << " " << i+1+start << endl;
		}
		start += off;

		return out.str();
	}

	void obj_writer::write_path(path path) {
		out << path.obj_string(start) << endl;
	}

	void obj_writer::write_icosphere(icosphere ico) {
		out << ico.obj_string(start) << endl;
	}
}