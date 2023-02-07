#include "find-hit.h"

#include "libgi/timer.h"

#ifndef RTGI_SIMPLER_BBVH
#include "rt/cpu/bvh-ctor.h"
#endif

#include "bindings.h"
#include "platform.h"

#include <iostream>
#include <GL/glew.h>

using namespace std;

namespace wf {
	namespace gl {

		/* 
		 *  sequential triangle intersector
		 *
		 */
		
		extern compute_shader seq_any_shader;
		extern compute_shader seq_closest_shader;

		seq_tri_is::seq_tri_is() {
			seq_closest_shader.bind();
			seq_any_shader.bind();
		}
		
		void seq_tri_is::compute_closest_hit() {
			bind_texture_as_image bind_r(rd->rays, 0, true, false);
			bind_texture_as_image bind_i(rd->intersections, 1, false, true);
			auto res = rc->resolution();
			seq_closest_shader.bind();
			seq_closest_shader.uniform("w", res.x).uniform("h", res.y);
			seq_closest_shader.uniform("N", (int) pf->sd->triangles.size);
			seq_closest_shader.dispatch(res.x, res.y);
			seq_closest_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}
		
		void seq_tri_is::compute_any_hit() {
			bind_texture_as_image bind_r(rd->rays, 0, true, false);
			bind_texture_as_image bind_i(rd->intersections, 1, false, true);
			auto res = rc->resolution();
			seq_any_shader.bind();
			seq_any_shader.uniform("w", res.x).uniform("h", res.y);
			seq_any_shader.uniform("N", (int) pf->sd->triangles.size);
			seq_any_shader.dispatch(res.x, res.y);
			seq_any_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}


#ifndef RTGI_SIMPLER_BBVH
		/*
		 * bvh adaptor for rt/bbvh-base
		 *
		 */
		struct cpu_bvh_builder_opengl_scene_traits {
			scenedata *s;
			typedef ivec4 tri_t;
			int  triangles() { return s->triangles.org_data.size(); }
			tri_t triangle(int index) { return s->triangles.org_data[index]; }
			int triangle_a(int index) { return s->triangles.org_data[index].x; }
			int triangle_b(int index) { return s->triangles.org_data[index].y; }
			int triangle_c(int index) { return s->triangles.org_data[index].z; }
			glm::vec3 vertex_pos(int index) { return s->vertices.org_data[index].pos; }
			void replace_triangles(std::vector<tri_t> &&new_tris) {
				s->triangles.org_data = new_tris;
			}
		};
		
		extern compute_shader bvh_closest_shader;

		bvh::bvh() : nodes("nodes", BIND_NODE, 0), indices("tri_index", BIND_TIDS, 0) {
			bvh_closest_shader.bind();
			// TODO anyhit
		}

		void bvh::build(scenedata *scene) {
			cpu_bvh_builder_opengl_scene_traits st { scene };
			bvh_ctor<bbvh_triangle_layout::indexed, cpu_bvh_builder_opengl_scene_traits> *ctor = nullptr;
			ctor = new bvh_ctor_sah<bbvh_triangle_layout::indexed, cpu_bvh_builder_opengl_scene_traits>(st, 4, 16);
			::bvh bvh = ctor->build(true);

			// sadly, we have to re-arrange the data to have properly aligned glsl values
			glsl_bvh_node *n = new glsl_bvh_node[bvh.nodes.size()];
			bbvh_node *org = bvh.nodes.data();
			#pragma omp parallel for
			for (int i = 0; i < bvh.nodes.size(); ++i) {
				bbvh_node &node = org[i];
				n[i].box1min_l = vec4(node.box_l.min, node.inner()?node.link_l:0);
				n[i].box1max_r = vec4(node.box_l.max, node.inner()?node.link_r:0);
				n[i].box2min_o = vec4(node.box_r.min, node.inner()?0:node.tri_offset());
				n[i].box2max_c = vec4(node.box_r.max, node.inner()?0:node.tri_count());
			}
			nodes.resize(bvh.nodes.size(), n);
			indices.resize(bvh.index.size(), bvh.index.data());
			glFinish();
		}

		void bvh::compute_closest_hit() {
			glFinish();
			glMemoryBarrier(GL_ALL_BARRIER_BITS);
			
			bind_texture_as_image bind_r(rd->rays, 0, true, false);
			bind_texture_as_image bind_i(rd->intersections, 1, false, true);
			auto res = rc->resolution();
			bvh_closest_shader.bind();
			bvh_closest_shader.uniform("w", res.x).uniform("h", res.y);
			bvh_closest_shader.dispatch(res.x, res.y);
			bvh_closest_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}

		void bvh::compute_any_hit() {
			throw std::logic_error("OpenGL Any-Hit ist not implemented, yet");
			// TODO
		}
#else
#warning "GL tracing not supported with tutorial's tracer"
		bvh::bvh() : nodes("nodes", BIND_NODE, 0), indices("tri_index", BIND_TIDS, 0) {
			throw std::logic_error("There is no viable implementation as this version of the code relies on an incompatible tracer");
		}
		void bvh::build(scenedata *scene) {}
		void bvh::compute_closest_hit() {}
		void bvh::compute_any_hit() {}

#endif
	}
}
