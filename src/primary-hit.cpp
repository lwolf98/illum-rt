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
		glm::vec3 radiance(0);
		ray ray = cam_ray(rc.scene.camera, x, y, glm::vec2(rc.rng.uniform_float()-0.5f, rc.rng.uniform_float()-0.5f));
		triangle_intersection closest = rc.scene.rt->closest_hit(ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc.scene);
			if (dg.mat->emissive != vec3(0)) {
				radiance = dg.mat->emissive;
			}
			else {
				//auto col = dg.mat->albedo_tex ? dg.mat->albedo_tex->sample(dg.tc) : dg.mat->albedo;
				lambertian_reflection d_brdf;
				phong_specular_reflection s_brdf;
				layered_brdf brdf(&s_brdf, &d_brdf);
				auto [l_id, l_pdf] = rc.scene.light_distribution->sample_index(rc.rng.uniform_float());
				light *l = rc.scene.lights[l_id];
				auto [shadow_ray,col,pdf] = l->sample_Li(dg, rc.rng.uniform_float2());
				if (auto is = rc.scene.rt->closest_hit(shadow_ray); !is.valid() || is.t >= shadow_ray.max_t) {
					radiance = col * brdf.f(dg, -ray.d, shadow_ray.d) * cdot(shadow_ray.d, dg.ns) / (pdf * l_pdf);
				}
			}
		}
		result.push_back({radiance,vec2(0)});
	}
	return result;
}
