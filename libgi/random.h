#pragma once

#include "rt.h"

#include <random>
#include <vector>
#include <glm/glm.hpp>

class rng {
	// those are mutable because they are put in the render_context which is passed const to the gi algorithms.
    mutable std::vector<std::mt19937> per_thread_rng;
    mutable std::uniform_real_distribution<float> uniform_float_distribution{0, 1.f};
    mutable std::uniform_int_distribution<uint32_t> uniform_uint_distribution{0, UINT_MAX};
    
public:
	float uniform_float() const;
    uint32_t uniform_uint() const;
 
	rng();
	rng(const rng&) = delete;
	rng& operator=(const rng&) = delete;
	rng(rng &&other) = default;

	vec2 uniform_float2() const;
};
