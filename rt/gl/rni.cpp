#include "rni.h"

#include <iostream>
using namespace std;

namespace wf {
	namespace gl {

		batch_cam_ray_setup::batch_cam_ray_setup()
		: cs("batch_cam_ray_setup",
			 platform::standard_preamble +
			 R"(
			   uniform vec3 p, d, U, V;
			   uniform vec2 near_wh;
			   void run(uint x, uint y) {
			   		uint id = y * w + x;
			   		vec2 offset = vec2(0,0);
			   		float u = (float(x)+0.5+offset.x)/float(w) * 2.0f - 1.0f;	// \in (-1,1)
			   		float v = (float(y)+0.5+offset.y)/float(h) * 2.0f - 1.0f;
			   		u = near_wh.x * u;	// \in (-near_w,near_w)
			   		v = near_wh.y * v;
			   		vec3 dir = normalize(d + U*u + V*v);
			   		rays_o[id] = vec4(p, 1);
			   		rays_d[id] = vec4(dir, 0);
			   		rays_id[id] = vec4(vec3(1)/dir, 1);
			   }
			 )") {
			cs.compile();
		}
		void batch_cam_ray_setup::run() {
			auto res = rc->resolution();
			camera &cam = rc->scene.camera;
			vec3 U = cross(cam.dir, cam.up);
			vec3 V = cross(U, cam.dir);
			
			cs.bind();
			cs.uniform("w", res.x).uniform("h", res.y);
			cs.uniform("p", cam.pos).uniform("d", cam.dir).uniform("U", U).uniform("V", V);
			cs.uniform("near_wh", cam.near_w, cam.near_h);
			cs.dispatch(res.x, res.y);
			cs.unbind();
		}
		
		void store_hitpoint_albedo::run() {
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

