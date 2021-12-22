#include "rni.h"

#include "libgi/timer.h"

#include <iostream>
using namespace std;

namespace wf {
	namespace gl {

		extern compute_shader ray_setup_shader;

		void batch_cam_ray_setup::run() {
			time_this_block(batch_cam_setup);
			auto res = rc->resolution();
			camera &cam = rc->scene.camera;
			vec3 U = cross(cam.dir, cam.up);
			vec3 V = cross(U, cam.dir);
			
			compute_shader &cs = ray_setup_shader;
			cs.bind();
			cs.uniform("w", res.x).uniform("h", res.y);
			cs.uniform("p", cam.pos).uniform("d", cam.dir).uniform("U", U).uniform("V", V);
			cs.uniform("near_wh", cam.near_w, cam.near_h);
			cs.dispatch(res.x, res.y);
			cs.unbind();
		}
		
		void store_hitpoint_albedo::run() {
			time_this_block(download_hitpoints);
			auto res = rc->resolution();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

			// this conversion is going to be a pain
			auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
			rt->rd.intersections.download();
			triangle_intersection *is = (triangle_intersection*)rt->rd.intersections.org_data.data();

			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					vec3 radiance(0);
					triangle_intersection &hit = is[y*res.x+x];
					if (hit.valid()) {
						diff_geom dg(hit, rc->scene);
						radiance += dg.albedo();
					}
					//radiance *= one_over_samples;
					rc->framebuffer.color(x,y) = vec4(radiance, 1);
				}
		}
	
	}
}

