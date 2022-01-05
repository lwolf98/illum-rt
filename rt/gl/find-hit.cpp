#include "find-hit.h"

#include "libgi/timer.h"

#include <iostream>
#include <GL/glew.h>

using namespace std;

namespace wf {
	namespace gl {

		/* 
		 *  sequential triangle intersector
		 *
		 */

		
		void seq_tri_is::compute_closest_hit() {
			time_this_block_gpu(seq_tri_is_closest_hit);
			auto res = rc->resolution();
			extern compute_shader seq_closest_shader;
			seq_closest_shader.bind();
			seq_closest_shader.uniform("w", res.x).uniform("h", res.y);
			seq_closest_shader.uniform("N", rc->scene.triangles.size());
			seq_closest_shader.dispatch(res.x, res.y);
			seq_closest_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}
		
		void seq_tri_is::compute_any_hit() {
			time_this_block_gpu(seq_tri_is_any_hit);
			auto res = rc->resolution();
			extern compute_shader seq_any_shader;
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

		bvh::bvh() : nodes("nodes", 8, 0), indices("tri_index", 9, 0) {
		}

		void bvh::build(::scene *scene) {
			batch_rt::build(scene);

			binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on> backend;
			backend.build(scene);

			cout << "nodes = " << backend.nodes.size() << endl;
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
			cout << backend.nodes[0].link_l << " links " << backend.nodes[0].link_r << endl;
			cout << n[0].box1min_l.w << " l---s " << n[0].box1max_r.w << endl;
			nodes.resize(backend.nodes.size(), n);
			indices.resize(backend.index.size(), backend.index.data());
		}

		void bvh::compute_closest_hit() {
			glFinish();
			glMemoryBarrier(GL_ALL_BARRIER_BITS);
			
			time_this_block_gpu(bvh_closest_hit);
			auto res = rc->resolution();
			extern compute_shader bvh_closest_shader;
			bvh_closest_shader.bind();
			bvh_closest_shader.uniform("w", res.x).uniform("h", res.y);
			bvh_closest_shader.uniform("N", rc->scene.triangles.size());
			bvh_closest_shader.dispatch(res.x, res.y);
			bvh_closest_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}

		void bvh::compute_any_hit() {
		}
	}
}
