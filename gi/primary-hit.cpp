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

#include "debug/pixel.h"
#include "debug/obj_writer.h"

using namespace glm;
using namespace std;

#define WITH_OBJ_DEBUG
#ifdef WITH_OBJ_DEBUG
def_obj_debug;
#endif

vec3 primary_hit_display::sample_pixel(uint32_t x, uint32_t y) {
#ifndef RTGI_SKIP_PRIM_HIT_IMPL
	vec3 radiance(0);
	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (closest.valid()) {
		diff_geom dg = diff_geom::init(closest, rc->scene);
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
#ifdef WITH_OBJ_DEBUG
	start_obj_debug(x, y, "/tmp/debug_" + std::to_string(current_sample_index) + ".obj");
	if (debug)
		*ow << obj::object("path");
#endif

	vec3 radiance(0);
	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (x == debug_pixel_x && y == debug_pixel_y)
		std::cout << "Camray: " << std::endl << closest.to_string() << std::endl;

	if (closest.valid()) {
		diff_geom dg = diff_geom::init(closest, rc->scene);

#ifdef WITH_OBJ_DEBUG
		if (x == debug_pixel_x && y == debug_pixel_y) {
			*ow << obj::line(view_ray.o, view_ray.o + view_ray.d * closest.t);
			if (rc->scene.is_patch(closest.ref)) {
				const subd::subd_patch &patch = rc->scene.patches[((uint32_t)-1) - closest.ref];
				triangle tri = patch.tri(closest.subd_quad_ref.ref(), closest.subd_quad_ref.is_upper_tri());
				const vertex &a = patch.verts[tri.a];
				const vertex &b = patch.verts[tri.b];
				const vertex &c = patch.verts[tri.c];
				*ow << obj::line(dg.dbg_v_a.pos, dg.dbg_v_b.pos);
				*ow << obj::line(dg.dbg_v_b.pos, dg.dbg_v_c.pos);
				*ow << obj::line(dg.dbg_v_c.pos, dg.dbg_v_a.pos);

				//*ow << obj::line(dg.dbg_g_a.pos, dg.dbg_g_b.pos);
				//*ow << obj::line(dg.dbg_g_b.pos, dg.dbg_g_c.pos);
				//*ow << obj::line(dg.dbg_g_c.pos, dg.dbg_g_a.pos);
			}
		}
#endif

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
		triangle_intersection is = rc->scene.rt->closest_hit(shadow_ray);
		if (x == debug_pixel_x && y == debug_pixel_y) {
			std::cout << "Shadowray: " << std::endl << is.to_string() << std::endl << std::endl;
			float t = is.t;
			if (!is.valid()) t = 10.f;
			*ow << obj::line(shadow_ray.o, shadow_ray.o + shadow_ray.d * t);
		}

		if (!is.valid())
			radiance = pl->power() * brdf->f(dg, w_o, w_i) / (d*d);
#else
		// todo: implement local illumination via the BRDF
		radiance = dg.albedo();
#endif
	}

	return radiance;
}
#endif

void local_illumination::finalize_frame() {
	finalize_obj_debug
}

#ifndef RTGI_SKIP_DEBUGALGO
vec3 info_display::sample_pixel(uint32_t x, uint32_t y) {
	vec3 radiance(0);
	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (closest.valid()) {
		diff_geom dg(closest, rc->scene);
		if (debug_pixel(x, y)) {
			cout << "Material: '" << dg.mat->name << "'" << endl;
		}
		radiance = dg.albedo();
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
