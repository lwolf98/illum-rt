#include "base.h"
#include "rni.h"
#include "seq.h"

#include <stdexcept>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

using std::vector;

namespace wf {
	namespace gl {


		timer::timer() {
		}

		void timer::start(const std::string &name) {
			GLuint q0;
			if (queries.find(name) == queries.end()) {
				GLuint q[2];
				glGenQueries(2, q);
				queries[name] = {q[0], q[1]};
				q0 = q[0];
			}
			else
				q0 = queries[name].first;
			glQueryCounter(q0, GL_TIMESTAMP);
		}
		
		void timer::stop(const std::string &name) {
			auto [q0,q1] = queries[name];
			glQueryCounter(q1, GL_TIMESTAMP);
			
			GLint available = GL_FALSE;
			while (available == GL_FALSE)
				glGetQueryObjectiv(q1, GL_QUERY_RESULT_AVAILABLE, &available);

			GLuint64 start, stop;
			glGetQueryObjectui64v(q0, GL_QUERY_RESULT, &start);
			glGetQueryObjectui64v(q1, GL_QUERY_RESULT, &stop);

			// funnel to stats_timer
			stats_timer.timers[0].times[name] += (stop - start);
			stats_timer.timers[0].counts[name]++;
		}

		timer gpu_timer;

		static GLFWwindow *window;

		void scenedata::upload(scene *scene) {
			triangles.resize(scene->triangles.size(), reinterpret_cast<ivec4*>(scene->triangles.data()));

			int N = scene->vertices.size();
			vector<vec4> v4(N);

			for (int i = 0; i < N; ++i)
				v4[i] = vec4(scene->vertices[i].pos, 1);
			vertex_pos.resize(v4);

			for (int i = 0; i < N; ++i)
				v4[i] = vec4(scene->vertices[i].norm, 0);
			vertex_norm.resize(v4);

			vector<vec2> v2(N);
			for (int i = 0; i < N; ++i)
				v2[i] = scene->vertices[i].tc;
			vertex_tc.resize(v2);
		}

		platform::platform() : wf::platform("opengl") {
			if (!glfwInit())
				throw std::runtime_error("cannot create glfw window required for fallback opengl context");
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

			window = glfwCreateWindow(1, 1, "Rasterizer", nullptr, nullptr);
			if (!window) {
				glfwTerminate();
				throw std::runtime_error("cannot create glfw window required for fallback opengl context");
			}

			glfwMakeContextCurrent(window);
			glfwSwapInterval(0);

			if (glewInit() != GLEW_OK) {
				glfwTerminate();
				throw std::runtime_error("cannot initialize glew");
			}
			
			
			tracers["default"] = new seq_tri_is;
			// bvh mode?
			rnis["setup camrays"] = new batch_cam_ray_setup;
			rnis["store hitpoint albedo"] = new store_hitpoint_albedo;
		}
		
		std::string platform::standard_preamble = R"(
			#version 450
			layout (local_size_x = 32, local_size_y = 32) in;
			layout (std430, binding = 0) buffer b_rays_o  { vec4 rays_o  []; };
			layout (std430, binding = 1) buffer b_rays_d  { vec4 rays_d  []; };
			layout (std430, binding = 2) buffer b_rays_id { vec4 rays_id []; };
			layout (std430, binding = 3) buffer b_intersections { vec4 intersections[]; };
			layout (std430, binding = 4) buffer b_vertex_pos  { vec4 vertex_pos []; };
			layout (std430, binding = 5) buffer b_vertex_norm { vec4 vertex_norm[]; };
			layout (std430, binding = 6) buffer b_vertex_tc   { vec4 vertex_tc  []; };
			layout (std430, binding = 7) buffer b_triangles   { ivec4 triangles []; };
			layout (std430, binding = 8) buffer b_radiance  { vec4 radiance[]; };
			uniform int w;
			uniform int h;
			void run(uint x, uint y);
			void main() {
				if (gl_GlobalInvocationID.x < w || gl_GlobalInvocationID.y < h)
					run(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
			}
			)";

	}
}
