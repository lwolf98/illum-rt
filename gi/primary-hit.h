#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"

/* \brief Display the color (albedo) of the surface closest to the given ray.
 *
 * - x, y are the pixel coordinates to sample a ray for.
 * - samples is the number of samples to take
 * - render_context holds contextual information for rendering (e.g. a random number generator)
 *
 */
class primary_hit_display : public gi_algorithm {
public:
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &r) override;
};

#ifndef RTGI_AXX
class direct_light : public gi_algorithm {
	lambertian_reflection d_brdf;
	phong_specular_reflection s_brdf;
	layered_brdf l_brdf;
	gtr2_reflection gtr2_brdf;
	layered_brdf l_brdf2;
	::brdf *brdf = nullptr;
	enum sampling_mode { sample_light, sample_brdf, both };
	enum sampling_mode sampling_mode = sample_light;
public:
	direct_light() : l_brdf(&s_brdf, &d_brdf), l_brdf2(&gtr2_brdf, &d_brdf), brdf(&d_brdf) {}
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &r) override;
	bool interprete(const std::string &command, std::istringstream &in) override;
};
#endif
