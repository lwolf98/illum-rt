#include "primary-hit.h"

#include "config.h"

#ifdef HAVE_GL
#include "driver/preview.h"
#endif

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"

#include "libgi/global-context.h"

using namespace glm;
using namespace std;

vec3 primary_hit_display::sample_pixel(uint32_t x, uint32_t y) {
#ifndef RTGI_SKIP_PRIM_HIT_IMPL
	vec3 radiance(0);
	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (closest.valid()) {
		diff_geom dg(closest, rc->scene);
		radiance = dg.albedo();
	}
	return radiance;
#else
	// todo: implement primary hitpoint algorithm
	return vec3(0);
#endif
}

#ifndef RTGI_SKIP_LOCAL_ILLUM
vec3 local_illumination::sample_pixel(uint32_t x, uint32_t y) {
	vec3 radiance(0);
	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
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
		if (!rc->scene.rt->any_hit(shadow_ray))
			radiance = pl->power() * brdf->f(dg, w_o, w_i) / (d*d);
#else
		// todo: implement local illumination via the BRDF
		radiance = dg.albedo();
#endif
	}
	return radiance;
}
#endif

#ifndef RTGI_SKIP_WF
namespace wf {
	primary_hit_display::primary_hit_display() {
		auto *init_fb = rc->platform->step<initialize_framebuffer>();
		auto *download_fb = rc->platform->step<download_framebuffer>();
		frame_preparation_steps.push_back(init_fb);
		frame_finalization_steps.push_back(download_fb);

		auto *sample_cam = rc->platform->step<sample_camera_rays>();
		auto *find_hit   = rc->platform->step<find_closest_hits>();
		auto *add_albedo = rc->platform->step<add_hitpoint_albedo>();

		rd = rc->platform->allocate_raydata();

		sampling_steps.push_back(sample_cam);
		sampling_steps.push_back(find_hit);
		sampling_steps.push_back(add_albedo);

#ifdef HAVE_GL
		if (preview_window) {
			auto *copy_prev = rc->platform->step<copy_to_preview>();
			sampling_steps.push_back(copy_prev);
			copy_prev->use(rd);
		}
#endif

		init_fb->use(rd);
		download_fb->use(rd);

		sample_cam->use(rd);
		find_hit->use(rd);
		add_albedo->use(rd);
	}
}
#endif
