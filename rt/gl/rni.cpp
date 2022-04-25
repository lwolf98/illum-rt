#include "rni.h"

#include "libgi/timer.h"
#include "libgi/random.h"

#include <iostream>
using namespace std;


#include <unistd.h>

namespace wf {
	namespace gl {

		extern compute_shader ray_setup_shader;
		extern compute_shader clear_framebuffer_shader;
		extern compute_shader add_hitpoint_albedo_shader;
		extern compute_shader add_hitpoint_albedo_hackytex_shader;
		extern compute_shader add_hitpoint_albedo_plain_shader;

		void initialize_framebuffer::run() {
			auto res = rc->resolution();
			compute_shader &cs = clear_framebuffer_shader;
			cs.bind();
			cs.uniform("w", res.x).uniform("h", res.y);
			cs.dispatch(res.x, res.y);
			cs.unbind();
		}
		
		void download_framebuffer::run() {
// 			glFinish();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
			auto res = rc->resolution();
			rt->rd->framebuffer.download();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					vec4 c = rt->rd->framebuffer.org_data[y*res.x+x];
					rc->framebuffer.color(x,y) = c / c.w;
				}
		}

		batch_cam_ray_setup::batch_cam_ray_setup() : pcg_data("ray gen rng", BIND_RRNG, 0) {
			rc->call_at_resolution_change[this] = [this](int w, int h) {
				init_pcg_data(w, h);
			};
			if (rc->resolution().x > 0 && rc->resolution().y > 0)
				init_pcg_data(rc->resolution().x, rc->resolution().y);
		}
		
		batch_cam_ray_setup::~batch_cam_ray_setup() {
			rc->call_at_resolution_change.erase(this);
		}
		
		void batch_cam_ray_setup::init_pcg_data(int w, int h) {
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
		
		void add_hitpoint_albedo::run() {
// 			glFinish();
			//time_this_block(add_hitpoint_albedo);
			auto res = rc->resolution();
			compute_shader *cs = &add_hitpoint_albedo_plain_shader;
			if (texture_support_mode == PROPER_BINDLESS)
				cs = &add_hitpoint_albedo_shader;
			else if (texture_support_mode == HACKY)
				cs = &add_hitpoint_albedo_hackytex_shader;
			cs->bind();
			cs->uniform("w", res.x).uniform("h", res.y);
			cs->dispatch(res.x, res.y);
			cs->unbind();

// 			int x;
// 			glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &x);
// 			cout << "MAX ::: " << x << endl;

		}
	
	}
}

