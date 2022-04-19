#include "wavefront.h"
#include "libgi/timer.h"

#include "seq.h"
#include "bvh.h"

#include "config.h"

#ifdef HAVE_LIBEMBREE3
#include "embree.h"
#endif

namespace wf {
	namespace cpu {

		// raydata

		raydata::raydata(int w, int h) : w(w), h(h) {
			if (w > 0 && h > 0) {
				rays = new ray[w*h];
				intersections = new triangle_intersection[w*h];
			}
			rc->call_at_resolution_change[this] = [this](int new_w, int new_h) {
				delete [] rays;
				delete [] intersections;
				this->w = new_w;
				this->h = new_h;
				rays = new ray[this->w*this->h];
				intersections = new triangle_intersection[this->w*this->h];
			};
		}
		raydata::~raydata() {
			rc->call_at_resolution_change.erase(this);
			delete [] rays;
			delete [] intersections;
		}

		// batch_rt_adapter

		void batch_rt_adapter::compute_closest_hit() {
			time_this_block(closest_hit);
			glm::ivec2 res = rc->resolution();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)  // ray data missing
					rd.intersections[y*res.x+x] = underlying_rt->closest_hit(rd.rays[y*res.x+x]);
		}
		void batch_rt_adapter::compute_any_hit() {
			time_this_block(any_hit);
			glm::ivec2 res = rc->resolution();	
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)
					rd.intersections[y*res.x+x] = underlying_rt->any_hit(rd.rays[y*res.x+x]);
		}

		void batch_rt_adapter::build(::scene *s) {
			underlying_rt->build(s);
		}

		// RNI STEPS

		void initialize_framebuffer::run() {
			time_this_block(initialize_framebuffer);
			auto res = rc->resolution();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)
					rc->framebuffer.color(x,y) = vec4(0,0,0,0);
		}

		void batch_cam_ray_setup_cpu::run() {
			time_this_block(setup__camrays);
			auto res = rc->resolution();
			auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
			assert(rt != nullptr);
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
					rt->rd.rays[y*res.x+x] = view_ray;
				}
		}
			
		// store_hitpoint_albedo

		void add_hitpoint_albedo::run() {
			time_this_block(add_hitpoint_albedo);
			auto res = rc->resolution();
			float one_over_samples = 1.0f/rc->sppx;
			auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
			assert(rt != nullptr);
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					vec3 radiance(0);
					for (int sample = 0; sample < rc->sppx; ++sample) {
						triangle_intersection closest = rt->rd.intersections[y*res.x+x];
						if (closest.valid()) {
							diff_geom dg(closest, rc->scene);
							radiance += dg.albedo();
						}
					}
					radiance *= one_over_samples;
					rc->framebuffer.color(x,y) += vec4(radiance, 1);
				}
		}
			
		void download_framebuffer::run() {
			time_this_block(download_framebuffer);
			auto res = rc->resolution();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)
					rc->framebuffer.color(x,y) /= rc->framebuffer.color(x,y).w;
		}

		// THE PLATFORM

		platform::platform(const std::vector<std::string> &args) : wf::platform("cpu") {
			for (auto arg : args)
				std::cerr << "Platform opengl does not support the argument " << arg << std::endl;
			cpu::raydata *rd = new cpu::raydata(rc->resolution());
			raydata = rd;

			register_batch_rt("seq", rd, batch_rt_adapter(new seq_tri_is, rd));
			register_batch_rt("bbvh-esc", rd, batch_rt_adapter(new binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>, rd));

#ifdef HAVE_LIBEMBREE3
			register_batch_rt("embree", rd, batch_rt_adapter(new embree_tracer, rd));
			link_tracer("embree", "default");
#else
#ifndef RTGI_SKIP_BVH
			link_tracer("bbvh-esc", "default");
#else
			link_tracer("seq", "default");
#endif
#endif

			// bvh mode?
			register_rni_step("initialize framebuffer",, initialize_framebuffer);
			register_rni_step("setup camrays",, batch_cam_ray_setup_cpu);
			register_rni_step("add hitpoint albedo",, add_hitpoint_albedo);
			register_rni_step("download framebuffer",, download_framebuffer);
		}

	}
}
