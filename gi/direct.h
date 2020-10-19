#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"

#ifndef RTGI_AXX
class direct_light : public gi_algorithm {
	enum sampling_mode { sample_light, sample_brdf, both };
	enum sampling_mode sampling_mode = sample_light;
public:
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &r) override;
	bool interprete(const std::string &command, std::istringstream &in) override;
};
#endif
