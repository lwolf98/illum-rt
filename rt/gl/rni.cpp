#include "rni.h"

#include "platform.h"
#include "config.h"

#ifdef HAVE_GL
#include "driver/preview.h"
#endif

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
		extern compute_shader copy_to_preview_shader;

		initialize_framebuffer::initialize_framebuffer() {
			clear_framebuffer_shader.bind();
		}
		void initialize_framebuffer::run() {
			time_this_wf_step;
			auto res = rc->resolution();
			bind_texture_as_image bind_f(rd->framebuffer, 2, false, true);
			compute_shader &cs = clear_framebuffer_shader;
			cs.bind();
			cs.uniform("w", res.x).uniform("h", res.y);
			cs.dispatch(res.x, res.y);
			cs.unbind();
		}
		
		void download_framebuffer::run() {
// 			glFinish();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
			time_this_wf_step;
			auto res = rc->resolution();
			rd->framebuffer.download();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					vec4 c = rd->framebuffer.org_data[y*res.x+x];
					rc->framebuffer.color(x,y) = c / c.w;
				}
		}

		void copy_to_preview::run() {
#ifdef HAVE_GL
			if (!preview_window) return;

			time_this_wf_step;
			auto res = rc->resolution();
			bind_texture_as_image bind_f(rd->framebuffer, 2, false, true);
			compute_shader &cs = copy_to_preview_shader;
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (GLuint)preview_framebuffer->index, preview_framebuffer->id);
			cs.bind();
			cs.uniform("w", res.x).uniform("h", res.y);
			cs.dispatch(res.x, res.y);
			cs.unbind();
#endif
		}

		batch_cam_ray_setup::batch_cam_ray_setup() : rng("ray gen") {
			rc->call_at_resolution_change[this] = [this](int w, int h) {
				rng.init_pcg_data_host(w, h);
			};
			if (rc->resolution().x > 0 && rc->resolution().y > 0)
				rng.init_pcg_data_host(rc->resolution().x, rc->resolution().y);
			ray_setup_shader.bind();
		}
		
		batch_cam_ray_setup::~batch_cam_ray_setup() {
			rc->call_at_resolution_change.erase(this);
		}
		

		void batch_cam_ray_setup::run() {
			time_this_wf_step;
			auto res = rc->resolution();
			camera &cam = rc->scene.camera;
			vec3 U = normalize(cross(cam.dir, cam.up));
			vec3 V = normalize(cross(U, cam.dir));
			
			bind_texture_as_image bind_r(rd->rays, 0, false, true);
			compute_shader &cs = ray_setup_shader;
			cs.bind();
			cs.uniform("w", res.x).uniform("h", res.y);
			cs.uniform("p", cam.pos).uniform("d", cam.dir).uniform("U", U).uniform("V", V);
			cs.uniform("near_wh", cam.near_w, cam.near_h);
			cs.dispatch(res.x, res.y);
			cs.unbind();
		}

		find_closest_hits::find_closest_hits() : wf::wire::find_closest_hits<raydata>(pf->rt) {
		}

		find_any_hits::find_any_hits() : wf::wire::find_any_hits<raydata>(pf->rt) {
		}

		add_hitpoint_albedo::add_hitpoint_albedo() {
			cs = &add_hitpoint_albedo_plain_shader;
			if (texture_support_mode == PROPER_BINDLESS)
				cs = &add_hitpoint_albedo_shader;
			else if (texture_support_mode == HACKY)
				cs = &add_hitpoint_albedo_hackytex_shader;
			cs->bind();
		}	
		void add_hitpoint_albedo::run() {
			time_this_wf_step;
// 			glFinish();
			//time_this_block(add_hitpoint_albedo);
			bind_texture_as_image bind_i(sample_rays->intersections, 1, true, false);
			bind_texture_as_image bind_f(sample_rays->framebuffer, 2, true, true);
			auto res = rc->resolution();
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

