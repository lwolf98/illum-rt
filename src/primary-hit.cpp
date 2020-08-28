#include "primary-hit.h"

#include "rt.h"
#include "context.h"
#include "intersect.h"
#include "util.h"

using namespace glm;
using namespace std;

gi_algorithm::sample_result primary_hit_display::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &rc) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		ray ray = cam_ray(rc.scene.camera, x, y, glm::vec2(rc.rng.uniform_float()-0.5f, rc.rng.uniform_float()-0.5f));
		triangle_intersection closest = rc.scene.rt->closest_hit(ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc.scene);
			//auto col = dg.mat->albedo_tex ? dg.mat->albedo_tex->sample(dg.tc) : dg.mat->albedo;
			lambertian_reflection brdf;
			vec3 col = brdf.f(dg, -ray.d, rc.scene.up) * cdot(-ray.d, rc.scene.up);
			result.push_back({col,vec2(0)});
		}
		else
			result.push_back({vec3(0),vec2(0)});
	}
	return result;
}
