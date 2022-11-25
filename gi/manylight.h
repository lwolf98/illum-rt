#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"
#include "libgi/scene.h"
#include <vector>

// virtual point light
struct vpl : public pointlight {
    //vec3 throughput;
    diff_geom geometry;
    vec3 in;

    vpl(const vec3 pos, const vec3 col, diff_geom dg, vec3 in)
        : pointlight(pos, col), geometry(dg), in(in) {}

    //vpl(const vec3 pos, const vec3 col)
    //    : pointlight(pos, col) {}
};

class manylight_algorithm : public recursive_algorithm {
private:
    std::vector<vpl> vpls;

public:
	void prepare_frame() override;
    vec3 sample_pixel(uint32_t x, uint32_t y) override;
    ~manylight_algorithm();
};

class russian_roulette {
    private:
        const int start;
        const int max;
        const int max_hot;
        int cold_count;
        int hot_count;

    public:
        russian_roulette(int max, int start) :
            max(max), start(start), cold_count(1), hot_count(0), max_hot(max - start + 1) {

        }
        russian_roulette(int max)
            : russian_roulette(max, 1) {
        }

        bool shot();
        void reset();
};