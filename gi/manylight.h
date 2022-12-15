#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"
#include "libgi/scene.h"
#include <vector>

#include "libgi/global-context.h"

// virtual point light
struct vpl : public pointlight {
	vpl* prev_vpl = nullptr;
	uint32_t vpl_index = 0;
	diff_geom geometry;
	vec3 w_in;

	// Constructor for following VPLs (v_1 to v_...)
	vpl(const vec3& col, const diff_geom& dg, const vec3& in, vpl* prev_vpl) : pointlight(dg.x, col), geometry(dg), w_in(in), prev_vpl(prev_vpl) {
		if (!prev_vpl)
			vpl_index = 0;
		else
			vpl_index = prev_vpl->vpl_index+1;
	}

	// Constructor for first VPL in path (v_0)
	vpl(const vec3 col, const diff_geom dg) : vpl(col, dg, vec3(0), nullptr) {}
};

class manylight_algorithm : public recursive_algorithm {
private:
	uint32_t paths;
	uint32_t path_length;
	std::vector<vpl> vpls;

public:
	manylight_algorithm(uint32_t paths, uint32_t path_length) : paths(paths), path_length(path_length) {}

	void prepare_frame() override;
	vec3 sample_pixel(uint32_t x, uint32_t y) override;
};