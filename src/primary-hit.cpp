#include "primary-hit.h"

#include "rt.h"
#include "scene.h"
#include "intersect.h"

using namespace glm;
using namespace std;

gi_algorithm::sample_result primary_hit_display::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const scene &scene) {
	ray ray = cam_ray(scene.camera, x, y);
	triangle_intersection closest = scene.rt->closest_hit(ray);
	if (closest.valid()) {
		auto &mat = scene.material(closest.ref);
		auto col = mat.albedo_tex ? scene.sample_texture(closest, mat.albedo_tex) : mat.albedo;
		return {{col,vec2(0)}};
	}
	return {{vec3(0),vec2(0)}};
}
