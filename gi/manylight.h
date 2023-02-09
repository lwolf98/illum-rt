#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"
#include "libgi/scene.h"
#include "gi/direct.h"
#include <vector>

#include "libgi/global-context.h"

// virtual point light
struct vpl : public pointlight {
	diff_geom geometry;
	vec3 w_in;

	// Constructor for VPLs (v_1 to v_...)
	vpl(const vec3& col, const diff_geom& dg, const vec3& w_in)
	: pointlight(dg.x, col), geometry(dg), w_in(w_in) {}
};

class manylight_algorithm : public direct_light {
private:
	enum sampling_mode sampling_mode = sample_light;

	uint32_t paths;
	uint32_t path_length;
	uint32_t vpls_per_sample = 5;
	std::vector<vpl> vpls;

public:
	manylight_algorithm(uint32_t paths, uint32_t path_length) : paths(paths), path_length(path_length) {}

	/* Generate VPLs */
	void prepare_frame() override;

	/* Integration via the previously generated VPLs */
	vec3 sample_pixel(uint32_t x, uint32_t y) override;
};

#ifndef RTGI_SKIP_WF
#include "manylight-steps.h"
namespace wf {
	//TODO-ML: check if inheritance is correctly setup (fields, visibility, virtual, override)
	class manylight_algorithm : public direct_light {
		void regenerate_steps() override;
	public:
		manylight_algorithm();
		bool interprete(const std::string &command, std::istringstream &in) override;
	};
}
#endif