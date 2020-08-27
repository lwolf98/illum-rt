#include "primary-hit.h"

#include "rt.h"
#include "scene.h"
#include "intersect.h"

using namespace glm;
using namespace std;

gi_algorithm::sample_result primary_hit_display::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const scene &scene) {
	ray ray = cam_ray(scene.camera, x, y);
	triangle_intersection closest = scene.rt->closest_hit(ray);
	auto mat = scene.material(closest.ref);
	return {{mat.kd,vec2(0)}};
}
