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
			// todo: implement phong lighting
			radiance = dg.albedo();
#endif
		}
		result.push_back({radiance,vec2(0)});
	}
	return result;
}
#endif

namespace wf {

	primary_hit_display::primary_hit_display() {
		clear = rc->platform->rni(initialize_framebuffer::id);
		download = rc->platform->rni(download_framebuffer::id);
		steps.push_back(rc->platform->rni(sample_camera_rays::id));
		steps.push_back(new wf::find_closest_hits(rc->scene.batch_rt));
		steps.push_back(rc->platform->rni(add_hitpoint_albedo::id));
	}

	void primary_hit_display::prepare_frame() {
		clear->run();
	}
	void primary_hit_display::finalize_frame() {
		download->run();
		rc->platform->timer->synchronize();
	}

	void primary_hit_display::compute_samples() {

		for (int i = 0; i < rc->sppx; ++i) {
			for (auto *step : steps)
				step->run();
			rc->platform->timer->synchronize();
		}
	}

}
