#include "find-hit.h"

#include "libgi/timer.h"

#include "bindings.h"

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
			auto res = rc->resolution();
			seq_closest_shader.bind();
			seq_closest_shader.uniform("w", res.x).uniform("h", res.y);
			seq_closest_shader.uniform("N", rc->scene.triangles.size());
			seq_closest_shader.dispatch(res.x, res.y);
			seq_closest_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}
		
		void seq_tri_is::compute_any_hit() {
			auto res = rc->resolution();
			seq_any_shader.bind();
			seq_any_shader.uniform("w", res.x).uniform("h", res.y);
			seq_any_shader.uniform("N", rc->scene.triangles.size());
			seq_any_shader.dispatch(res.x, res.y);
			seq_any_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}


		/*
		 * bvh adaptor for rt/bbvh-base
		 *
		 */
			
		extern compute_shader bvh_closest_shader;

		bvh::bvh() : nodes("nodes", BIND_NODE, 0), indices("tri_index", BIND_TIDS, 0) {
			bvh_closest_shader.bind();
			// TODO anyhit
		}

		void bvh::build(::scene *scene) {
			batch_rt::build(scene);

			binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on> backend;
			backend.build(scene);

			// sadly, we have to re-arrange the data to have properly aligned glsl values
			glsl_bvh_node *n = new glsl_bvh_node[backend.nodes.size()];
			bbvh_node *org = backend.nodes.data();
			#pragma omp parallel for
			for (int i = 0; i < backend.nodes.size(); ++i) {
				bbvh_node &node = org[i];
				n[i].box1min_l = vec4(node.box_l.min, node.inner()?node.link_l:0);
				n[i].box1max_r = vec4(node.box_l.max, node.inner()?node.link_r:0);
				n[i].box2min_o = vec4(node.box_r.min, node.inner()?0:node.tri_offset());
				n[i].box2max_c = vec4(node.box_r.max, node.inner()?0:node.tri_count());
			}
			nodes.resize(backend.nodes.size(), n);
			indices.resize(backend.index.size(), backend.index.data());
			glFinish();
		}

		void bvh::compute_closest_hit() {
			glFinish();
			glMemoryBarrier(GL_ALL_BARRIER_BITS);
			
			auto res = rc->resolution();
			bvh_closest_shader.bind();
			bvh_closest_shader.uniform("w", res.x).uniform("h", res.y);
			bvh_closest_shader.uniform("N", rc->scene.triangles.size());
			bvh_closest_shader.dispatch(res.x, res.y);
			bvh_closest_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}

		void bvh::compute_any_hit() {
			// TODO
		}
	}
}
