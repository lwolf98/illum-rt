#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"

#ifndef RTGI_A07_REF
class simple_pt : public gi_algorithm {
protected:
	int max_path_len = 1;
	enum class bounce { uniform, cosine, brdf } bounce = bounce::uniform;

	virtual vec3 path(ray view_ray);
	std::tuple<ray,float> bounce_ray(const diff_geom &dg, const ray &to_hit);
public:
	simple_pt(const render_context &rc) : gi_algorithm(rc) {}
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &r) override;
	bool interprete(const std::string &command, std::istringstream &in) override;
};
#endif

