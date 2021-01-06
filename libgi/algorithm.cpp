#include "algorithm.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/util.h"
#include "libgi/sampling.h"
	
float gi_algorithm::uniform_float() const {
	return rc.rng.uniform_float();
}

glm::vec2 gi_algorithm::uniform_float2() const {
	return rc.rng.uniform_float2();
}

std::tuple<ray,float> gi_algorithm::sample_uniform_direction(const diff_geom &hit) const {
	// set up a ray in the hemisphere that is uniformly distributed
	vec2 xi = rc.rng.uniform_float2();
	float z = xi.x;
	float phi = 2*pi*xi.y;
	// z is cos(theta), sin(theta) = sqrt(1-cos(theta)^2)
	float sin_theta = sqrtf(1.0f - z*z);
	vec3 sampled_dir = vec3(sin_theta * cosf(phi),
							sin_theta * sinf(phi),
							z);
	
	vec3 w_i = align(sampled_dir, hit.ng);
	ray sample_ray(hit.x, w_i);
	return { sample_ray, one_over_2pi };
}

std::tuple<ray,float> gi_algorithm::sample_cosine_distributed_direction(const diff_geom &hit) const {
	vec2 xi = rc.rng.uniform_float2();
	vec3 sampled_dir = cosine_sample_hemisphere(xi);
	vec3 w_i = align(sampled_dir, hit.ng);
	ray sample_ray(hit.x, w_i);
	return { sample_ray, sampled_dir.z * one_over_pi };
}

std::tuple<ray,float> gi_algorithm::sample_brdf_distributed_direction(const diff_geom &hit, const ray &to_hit) const {
	auto [w_i, f, pdf] = hit.mat->brdf->sample(hit, -to_hit.d, rc.rng.uniform_float2());
	ray sample_ray(hit.x, w_i);
	return {sample_ray, pdf};
}
