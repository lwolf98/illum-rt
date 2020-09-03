#pragma once

#include "algorithm.h"
#include "material.h"

class primary_hit_display : public gi_algorithm {
public:
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &r) override;
};

class direct_light : public gi_algorithm {
	lambertian_reflection d_brdf;
	phong_specular_reflection s_brdf;
	layered_brdf l_brdf;
	gtr2_reflection gtr2_brdf;
	::brdf *brdf = nullptr;
	enum sampling_mode { sample_light, sample_brdf };
	enum sampling_mode sampling_mode = sample_light;
public:
	direct_light() : l_brdf(&s_brdf, &d_brdf), brdf(&d_brdf) { gtr2_brdf.coat = true;}
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &r) override;
	bool interprete(const std::string &command, std::istringstream &in) override;
};
