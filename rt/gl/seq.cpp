#include "seq.h"

#include "libgi/timer.h"

#include <iostream>
#include <GL/glew.h>

using namespace std;

namespace wf {
	namespace gl {

		void seq_tri_is::build(::scene *scene) {
			auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
			rt->sd.upload(scene);
		}
		
		void seq_tri_is::compute_closest_hit() {
			time_this_block(seq_tri_is_closest_hit);
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
			time_this_block(seq_tri_is_any_hit);
			auto res = rc->resolution();
			extern compute_shader seq_any_shader;
			seq_any_shader.bind();
			seq_any_shader.uniform("w", res.x).uniform("h", res.y);
			seq_any_shader.uniform("N", rc->scene.triangles.size());
			seq_any_shader.dispatch(res.x, res.y);
			seq_any_shader.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}
	}
}
