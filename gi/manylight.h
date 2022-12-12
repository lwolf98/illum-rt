#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"
#include "libgi/scene.h"
#include <vector>

// virtual point light
struct vpl : public pointlight {
	diff_geom geometry;
	vec3 w_in;

	vpl(const vec3 pos, const vec3 col, diff_geom dg, vec3 in) : pointlight(pos, col), geometry(dg), w_in(in) {}

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

class russian_roulette {
private:
	const int start = 1;
	const int max;
	const int max_hot;
	int cold_count = 1;
	int hot_count = 0;

public:
	russian_roulette(int max, int start) : max(max),
										   start(start),
										   max_hot(max - start + 1) {

	}
	russian_roulette(int max)
		: russian_roulette(max, 1) {
	}

	bool shot();
	void reset();
};