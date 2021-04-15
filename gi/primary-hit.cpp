#include "primary-hit.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"

#include "libgi/global-context.h"

using namespace glm;
using namespace std;

gi_algorithm::sample_result primary_hit_display::sample_pixel(uint32_t x, uint32_t y, uint32_t samples) {
	sample_result result;
#ifndef RTGI_A01
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0);
		ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
		triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc->scene);
			// radiance = dg.albedo();
			radiance = dg.mat->albedo;
		}
		result.push_back({radiance,vec2(0)});
	}
#else
	result.push_back({vec3(0),vec2(0)});
#endif
	return result;
}

#ifndef RTGI_A02
gi_algorithm::sample_result local_illumination::sample_pixel(uint32_t x, uint32_t y, uint32_t samples) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0);
		ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
		triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc->scene);
			brdf *brdf = dg.mat->brdf;
			assert(!rc->scene.lights.empty());
			pointlight *pl = dynamic_cast<pointlight*>(rc->scene.lights[0]);
			assert(pl);
#ifndef RTGI_A03
			vec3 to_light = pl->pos - dg.x;
			vec3 w_i = normalize(to_light);
			vec3 w_o = -view_ray.d;
			float d = sqrtf(dot(to_light,to_light));

			ray shadow_ray(dg.x, w_i);
			shadow_ray.length_exclusive(d);
			if (!rc->scene.rt->any_hit(shadow_ray))
				radiance = pl->power() * brdf->f(dg, w_o, w_i) / (d*d);
#else
			// todo
			radiance = dg.albedo();
#endif
		}
		result.push_back({radiance,vec2(0)});
	}
	return result;
}
#endif



void primary_hit_display_wf::compute_samples() {
	auto res = rc->resolution();
	float one_over_samples = 1.0f/rc->sppx;
	#pragma omp parallel for
	for (int y = 0; y < res.y; ++y)
		for (int x = 0; x < res.x; ++x) {
			vec3 radiance(0);
			for (int sample = 0; sample < rc->sppx; ++sample) {
				ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
				triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
				if (closest.valid()) {
					diff_geom dg(closest, rc->scene);
					radiance += dg.albedo();
				}
			}
			radiance *= one_over_samples;
			rc->framebuffer.color(x,y) = vec4(radiance, 1);
		}
}
