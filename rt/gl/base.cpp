#include "base.h"
#include "rni.h"
#include "find-hit.h"

#include "opengl.h"

#include <stdexcept>
#include <vector>

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

		void scenedata::upload(scene *scene) {
			triangles.resize(scene->triangles.size(), reinterpret_cast<ivec4*>(scene->triangles.data()));

			int N = scene->vertices.size();
			vector<vertex> v(N);

			for (int i = 0; i < N; ++i) {
				v[i].pos = vec4(scene->vertices[i].pos, 1);
				v[i].norm = vec4(scene->vertices[i].norm, 0);
				v[i].tc = scene->vertices[i].tc;
			}
			vertices.resize(v);

			vector<material> mtl(scene->materials.size());
			for (int i = 0; i < scene->materials.size(); ++i) {
				material &m = mtl[i];
				m.emissive = vec4(scene->materials[i].emissive, 1);
				m.albedo = vec4(scene->materials[i].albedo, 1);
				std::cout << "material " << i << " albedo: " << m.albedo << std::endl;
				m.albedo_tex = 0;
				if (scene->materials[i].albedo_tex) {
					if (GLEW_ARB_bindless_texture) {
						m.has_tex = 1;
						GLuint tex;
						glGenTextures(1, &tex);
						glBindTexture(GL_TEXTURE_2D, tex);
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, scene->materials[i].albedo_tex->w, scene->materials[i].albedo_tex->h, 0, GL_RGB, GL_FLOAT, scene->materials[i].albedo_tex->texel);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
						glBindTexture(GL_TEXTURE_2D, 0);
						m.albedo_tex = glGetTextureHandleARB(tex);
						glMakeTextureHandleResidentARB(m.albedo_tex);
						textures.push_back(tex);
					}
					else {
						static bool have_warned_already = false;
						if (!have_warned_already)
							std::cerr << "You GPU or GL-version does not support ARB_bindless_texture, so textured materials will not work as expected" << std::endl;
					}
				}
				materials.resize(mtl);
			}
			glFinish();
		}
		
		scenedata::~scenedata() {
			materials.download();
			for (auto &m : materials.org_data)
				glMakeTextureHandleNonResidentARB(m.albedo_tex);
			for (auto tex : textures)
				glDeleteTextures(1, &tex);
		}

		platform::platform(const std::vector<std::string> &args) : wf::platform("opengl") {
			gl_mode requested_mode = gl_truly_headless;
			for (auto arg : args)
				if (arg == "gbm" || arg == "truly-headless")
					requested_mode = gl_truly_headless;
				else if (arg == "glfw" || arg == "with-X")
					requested_mode = gl_glfw_headless;
				else
					std::cerr << "Platform opengl does not support the argument " << arg << std::endl;
			if (gl_variant_available(requested_mode))
				initialize_opengl_context(requested_mode, 4, 4);
			else if (requested_mode != gl_glfw_headless)
				initialize_opengl_context(gl_glfw_headless, 4, 4);
			
			enable_gl_debug_output();

			register_batch_rt("seq",, seq_tri_is);
			register_batch_rt("bbvh-1",, bvh);
			link_tracer("bbvh-1", "default");
// 			link_tracer("seq", "default");
			// bvh mode?
			register_rni_step("initialize framebuffer",, initialize_framebuffer);
			register_rni_step("setup camrays",, batch_cam_ray_setup);
			register_rni_step("add hitpoint albedo",, add_hitpoint_albedo);
			register_rni_step("download framebuffer",, download_framebuffer);
		}
		
	}
}
