#include "base.h"
#include "rni.h"
#include "find-hit.h"

#include "opengl.h"

#include <stdexcept>
#include <vector>

using std::vector;

namespace wf {
	namespace gl {

		texture_support_mode_t texture_support_mode = PROPER_BINDLESS;

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
		}

		void timer::synchronize() {
			for (auto [name,qs] : queries) {
				auto [q0,q1] = qs;
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
			queries.clear();
		}
			
		rng::rng(const std::string &name) : pcg_data(name+" rng", BIND_RRNG, 0) {
		}

		void rng::init_pcg_data_host(int w, int h) {
			vector<uint64_t> data(w*h*2);
			#pragma omp parallel for
			for (int y = 0; y < h; ++y)
				for (int x = 0; x < w; ++x) {
					rng_pcg init(y*w+x);
					auto [s,i] = init.config();
					data[2*(y*w+x)+0] = s;
					data[2*(y*w+x)+1] = i;
				}
			pcg_data.resize(data);
		}
	
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
			vector<vec4> tex_data_hacky;
			for (int i = 0; i < scene->materials.size(); ++i) {
				material &m = mtl[i];
				m.emissive = vec4(scene->materials[i].emissive, 1);
				m.ior = scene->materials[i].ior;
				m.roughness = scene->materials[i].roughness;
				m.albedo = vec4(scene->materials[i].albedo, 1);
				m.albedo_tex = 0;
				if (scene->materials[i].albedo_tex) {
					if (texture_support_mode == PROPER_BINDLESS) {
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
					else if (texture_support_mode == HACKY) {
						m.has_tex = 1;
						m.albedo_tex = tex_data_hacky.size();
						texture2d<vec4> *src = scene->materials[i].albedo_tex;
						tex_data_hacky.push_back(vec4(src->w, src->h, 0, 0));
						for (int y = 0; y < src->h; ++y)
							for (int x = 0; x < src->w; ++x)
								tex_data_hacky.push_back(src->value(x, y));
					}
				}
				materials.resize(mtl);
			}
			if (texture_support_mode == HACKY)
				texture_data_hacky.resize(tex_data_hacky);
			glFinish();
		}
		
		scenedata::~scenedata() {
			materials.download();
			if (texture_support_mode == PROPER_BINDLESS) {
				for (auto &m : materials.org_data)
					glMakeTextureHandleNonResidentARB(m.albedo_tex);
				for (auto tex : textures)
					glDeleteTextures(1, &tex);
			}
		}

	}
}
