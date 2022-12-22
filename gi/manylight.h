#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"
#include "libgi/scene.h"
#include <vector>

#include "libgi/global-context.h"

// virtual point light
struct vpl : public pointlight {
	diff_geom geometry;
	vec3 w_in;

	// Constructor for following VPLs (v_1 to v_...)
	vpl(const vec3& col, const diff_geom& dg, const vec3& w_in)
	: pointlight(dg.x, col), geometry(dg), w_in(w_in) {}

	// Constructor for first VPL in path (v_0)
	vpl(const vec3& col, const diff_geom& dg) : vpl(col, dg, vec3(0)) {}
};

struct sample_context {
	vpl v;
	float pdf;

	sample_context(const vpl& v, float pdf) : v(v), pdf(pdf) {}
};

class manylight_algorithm : public recursive_algorithm {
private:
	uint32_t paths;
	uint32_t path_length;
	std::vector<vpl> vpls;
	std::vector<sample_context> sampled_lights;

public:
	manylight_algorithm(uint32_t paths, uint32_t path_length) : paths(paths), path_length(path_length) {}

	void prepare_frame() override;
	vec3 sample_pixel(uint32_t x, uint32_t y) override;
};