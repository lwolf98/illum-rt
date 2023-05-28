#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <map>
#include <glm/glm.hpp>


#include "libgi/material.h"
#include "libgi/scene.h"
#include "libgi/global-context.h"

#ifdef HAVE_CUDA
#include <vector_types.h>
#endif


namespace obj {
	struct material {
		std::string name;
		float Ns; 
		glm::vec3 Ka; 
		glm::vec3 Kd; 
		glm::vec3 Ks;
		glm::vec3 Ke;
		float Ni;
		float d;
		int illum;

		material(const std::string &name);
		material(const std::string &name,
				float Ns, const glm::vec3 &Ka, const glm::vec3 &Kd, const glm::vec3 &Ks, const glm::vec3 &Ke, float Ni, float d, int illum);
		
		material(const std::string &name, float r, float g, float b);
		material(float r, float g, float b);
		material(const ::material &mtl);
		
		static material hsv(const std::string &name, float h, float s, float v);
		static material hsv(float h, float s, float v);
		
		void write(std::ofstream &out);

		private:
			static int material_id;
	};


	struct triangle {

		triangle(const glm::vec3 &x, const glm::vec3 &y, const glm::vec3 &z) : x(x), y(y), z(z) {}
		triangle(const ::triangle &tri) : x(rc->scene.vertices[tri.a].pos), y(rc->scene.vertices[tri.b].pos), z(rc->scene.vertices[tri.c].pos) {}
#ifdef HAVE_CUDA
		triangle(const float4 &x, const float4 &y, const float4 &z) : x(x.x, x.y, x.z), y(y.x, y.y, y.z), z(z.x, z.y, z.z) {}
		triangle(const float3 &x, const float3 &y, const float3 &z) : x(x.x, x.y, x.z), y(y.x, y.y, y.z), z(z.x, z.y, z.z) {}
#endif
		
		glm::vec3 x, y, z;
	};

	struct index_triangle {
		index_triangle(unsigned int a, unsigned int b, unsigned int c) : a(a), b(b), c(c) {};
		unsigned int a, b, c;
	};

	struct line {
		line(const glm::vec3 &from, const glm::vec3 &to) : from(from), to(to) {}
		
#ifdef HAVE_CUDA
		line(const float4 &from, const float4 &to) : from(from.x, from.y, from.z), to(to.x, to.y, to.z) {}
		line(const float3 &from, const float3 &to) : from(from.x, from.y, from.z), to(to.x, to.y, to.z) {}
#endif
		
		glm::vec3 from, to;
	};

	struct wireframe_box {
		wireframe_box(const glm::vec3 &min, const glm::vec3 &max) : min(min), max(max) {};
#ifdef HAVE_CUDA
		wireframe_box(const float4 &min, const float4 &max) : min(min.x, min.y, min.z), max(max.x, max.y, max.z) {}
		wireframe_box(const float3 &min, const float3 &max) : min(min.x, min.y, min.z), max(max.x, max.y, max.z) {}
#endif
		glm::vec3 min, max;
	};

	struct solid_box {
		solid_box(const glm::vec3 &min, const glm::vec3 &max) : min(min), max(max) {};
#ifdef HAVE_CUDA
		solid_box(const float4 &min, const float4 &max) : min(min.x, min.y, min.z), max(max.x, max.y, max.z) {}
		solid_box(const float3 &min, const float3 &max) : min(min.x, min.y, min.z), max(max.x, max.y, max.z) {}
#endif
		glm::vec3 min, max;
	};

	struct icosphere {
		icosphere(const vec3& pos, float scale) : position(pos), scaling(scale) {}
		icosphere(const vec3& pos) : icosphere(pos, 1.0f) {}
		icosphere(float scale) : icosphere(vec3(0, 0, 0), scale) {}
		icosphere() : icosphere(vec3(0), 1.0f) {}
		icosphere& pos(const vec3 &p) { position = p; return *this; }
		glm::vec3 position;
		float scaling;
	};
	
	struct path {
		path(const std::vector<glm::vec3> &path_vertex_pos) : path_vertices(path_vertex_pos), highlight_path_vertices(false), scaling(1.0f) {}
#ifdef HAVE_CUDA
		path(const std::vector<float3> &path_vertex_pos) : highlight_path_vertices(false), scaling(1.0f) {
			path_vertices.reserve(path_vertex_pos.size());
			std::transform(path_vertex_pos.begin(), path_vertex_pos.end(), std::back_inserter(path_vertices), [] (const float3 &f) { return vec3(f.x, f.y, f.z); });
		}
		path(const std::vector<float4> &path_vertex_pos) : highlight_path_vertices(false), scaling(1.0f) {
			path_vertices.reserve(path_vertex_pos.size());
			std::transform(path_vertex_pos.begin(), path_vertex_pos.end(), std::back_inserter(path_vertices), [] (const float4 &f) { return vec3(f.x, f.y, f.z); });
		}
#endif
		path& highlight() { highlight_path_vertices = true; return *this;}
		path& scale(float scale) { scaling = scale; return *this;}
		std::vector<glm::vec3> path_vertices;
		bool highlight_path_vertices;
		float scaling;
	};

	struct object {
		object(const std::string &name) : name(name) {}
		std::string name;
	};

	struct group {
		group(const std::string &name) : name(name) {}
		std::string name;
	};

	class obj_writer {
	public:
		obj_writer(const std::filesystem::path &file_path);
		~obj_writer();

		int vertex(const glm::vec3 &v);
		int vertex(float x, float y, float z);

		obj_writer& operator<<(const obj::triangle &tri);
		obj_writer& operator<<(const obj::index_triangle &index_tri);
		obj_writer& operator<<(const obj::solid_box &sl_box);
		obj_writer& operator<<(const obj::wireframe_box &wf_box);
		obj_writer& operator<<(const obj::line &l);
		obj_writer& operator<<(const obj::icosphere &icosphere);
		obj_writer& operator<<(const obj::object &obj);
		obj_writer& operator<<(const obj::group &grp);
		obj_writer& operator<<(const obj::material &mat);
		obj_writer& operator<<(const obj::path &path);

	private:
		struct material_lib {
			std::filesystem::path file_path;
			std::map<std::string, obj::material> materials;
			material_lib(const std::filesystem::path &path);
			material_lib();
			void write();
			void add_material(const obj::material &material);
		};

		void close();
		void init_default_mtllib();

		unsigned int vertex_index;
		std::filesystem::path file_path;
		std::ofstream out;
		material_lib mtl_lib;
		std::string active_material;
	};

	extern const obj::material red;
	extern const obj::material green;
	extern const obj::material blue;	
}
