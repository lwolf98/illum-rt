#include "primary-hit.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"

#include "libgi/global-context.h"

#include "libgi/wavefront-rt.h"

using namespace glm;
using namespace std;

gi_algorithm::sample_result primary_hit_display::sample_pixel(uint32_t x, uint32_t y, uint32_t samples) {
	sample_result result;
#ifndef RTGI_SKIP_PRIM_HIT_IMPL
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0);
		ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
		triangle_intersection closest = rc->scene.single_rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc->scene);
			radiance = dg.albedo();
		}
		result.push_back({radiance,vec2(0)});
	}
#else
	// todo: implement primary hitpoint algorithm
	result.push_back({vec3(0),vec2(0)});
#endif
	return result;
}

#ifndef RTGI_SKIP_LOCAL_ILLUM
gi_algorithm::sample_result local_illumination::sample_pixel(uint32_t x, uint32_t y, uint32_t samples) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0);
		ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
		triangle_intersection closest = rc->scene.single_rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc->scene);
			brdf *brdf = dg.mat->brdf;
			assert(!rc->scene.lights.empty());
			pointlight *pl = dynamic_cast<pointlight*>(rc->scene.lights[0]);
			assert(pl);
#ifndef RTGI_SKIP_LOCAL_ILLUM_IMPL
			vec3 to_light = pl->pos - dg.x;
			vec3 w_i = normalize(to_light);
			vec3 w_o = -view_ray.d;
			float d = sqrtf(dot(to_light,to_light));

			ray shadow_ray(dg.x, w_i);
			shadow_ray.length_exclusive(d);
			if (!rc->scene.single_rt->any_hit(shadow_ray))
				radiance = pl->power() * brdf->f(dg, w_o, w_i) / (d*d);
#else
			// todo: implement local illumination via the BRDF
			radiance = dg.albedo();
#endif
		}
		result.push_back({radiance,vec2(0)});
	}
	return result;
}
#endif

#ifndef RTGI_SKIP_WF
namespace wf {
	namespace cpu {
		struct store_hitpoint_albedo : public batch_ray_and_intersection_processing_cpu {
			void run() override {
				auto res = rc->resolution();
				float one_over_samples = 1.0f/rc->sppx;
				auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
				assert(rt != nullptr);
				cout << res << endl;
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
		};
	}
}

void primary_hit_display_wf::compute_samples() {

// 	#pragma omp parallel for
// 	for (int y = 0; y < res.y; ++y)
// 		for (int x = 0; x < res.x; ++x)
// 			rays[y*w+x] = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
// 
	wf::cpu::batch_cam_ray_setup_cpu().run();
	auto *batch_rt = rc->scene.batch_rt;
	assert(batch_rt != nullptr);
	batch_rt->compute_closest_hit();

	wf::cpu::store_hitpoint_albedo().run();
}
#endif
