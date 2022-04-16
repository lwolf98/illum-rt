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

			vector<material> mtl(scene->materials.size());
			for (int i = 0; i < scene->materials.size(); ++i) {
				material &m = mtl[i];
				m.emissive = scene->materials[i].emissive;
				m.albedo = scene->materials[i].albedo;
				m.albedo_tex = 0;
				if (scene->materials[i].albedo_tex) {
					m.has_tex = true;
					GLuint tex;
					glGenTextures(1, &tex);
					glBindTexture(GL_TEXTURE_2D, tex);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, scene->materials[i].albedo_tex->w, scene->materials[i].albedo_tex->h, 0, GL_RGB32F, GL_FLOAT, scene->materials[i].albedo_tex->texel);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glBindTexture(GL_TEXTURE_2D, 0);
					m.albedo_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(m.albedo_tex);
					textures.push_back(tex);
				}
			}
		}
		
		scenedata::~scenedata() {
			materials.download();
			for (auto &m : materials.org_data)
				glMakeTextureHandleNonResidentARB(m.albedo_tex);
			for (auto tex : textures)
				glDeleteTextures(1, &tex);
		}

		platform::platform() : wf::platform("opengl") {
			if (gl_variant_available(gl_truly_headless))
				initialize_opengl_context(gl_truly_headless, 4, 4);
			else
				initialize_opengl_context(gl_glfw_headless, 4, 4);
			
			enable_gl_debug_output();

			register_batch_rt("seq",, seq_tri_is);
			register_batch_rt("bbvh-1",, bvh);
			link_tracer("bbvh-1", "default");
// 			link_tracer("seq", "default");
			// bvh mode?
			register_rni_step("setup camrays",, batch_cam_ray_setup);
			register_rni_step("store hitpoint albedo",, store_hitpoint_albedo);
		}
		
	}
}
