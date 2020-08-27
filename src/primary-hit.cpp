#include "primary-hit.h"

#include "rt.h"
#include "context.h"
#include "intersect.h"

using namespace glm;
using namespace std;

gi_algorithm::sample_result primary_hit_display::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &rc) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		ray ray = cam_ray(rc.scene.camera, x, y, glm::vec2(rc.rng.uniform_float()-0.5f, rc.rng.uniform_float()-0.5f));
		triangle_intersection closest = rc.scene.rt->closest_hit(ray);
		if (closest.valid()) {
			auto &mat = rc.scene.material(closest.ref);
			auto col = mat.albedo_tex ? rc.scene.sample_texture(closest, mat.albedo_tex) : mat.albedo;
			result.push_back({col,vec2(0)});
		}
		else
			result.push_back({vec3(0),vec2(0)});
	}
	return result;
}
