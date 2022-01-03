#include "wavefront-scalar.h"

#include "rt/bbvh-base/bvh.h"

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
			glm::ivec2 res = rc->resolution();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)  // ray data missing
					rd.intersections[y*res.x+x] = underlying_rt->closest_hit(rd.rays[y*res.x+x]);
		}
		void batch_rt_adapter::compute_any_hit() {
			glm::ivec2 res = rc->resolution();	
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)
					rd.intersections[y*res.x+x] = underlying_rt->any_hit(rd.rays[y*res.x+x]);
		}

		void batch_rt_adapter::build(::scene *s) {
			underlying_rt->build(s);
		}

		// batch_cam_ray_setup_cpu

		void batch_cam_ray_setup_cpu::run() {
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

		void store_hitpoint_albedo::run() {
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
					rc->framebuffer.color(x,y) = vec4(radiance, 1);
				}
		}

		// THE PLATFORM

		scalar_cpu_batch_raytracing::scalar_cpu_batch_raytracing() : platform("scalar-cpu") {
			cpu::raydata *rd = new cpu::raydata(rc->resolution());
			raydata = rd;

			register_batch_rt("default", batch_rt_adapter(new binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>, rd));
			// bvh mode?
			register_rni_step("setup camrays", batch_cam_ray_setup_cpu);
			register_rni_step("store hitpoint albedo", store_hitpoint_albedo);
		}

	}
}
