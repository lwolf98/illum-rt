#pragma once

#include "rt.h"

#include <random>
#include <vector>
#include <glm/glm.hpp>

class rng_std_mt {
	// those are mutable because they are put in the render_context which is passed const to the gi algorithms.
    mutable std::vector<std::mt19937> per_thread_rng;
    mutable std::uniform_real_distribution<float> uniform_float_distribution{0, 1.f};
    mutable std::uniform_int_distribution<uint32_t> uniform_uint_distribution{0, UINT_MAX};
    
public:
	float uniform_float() const;
    uint32_t uniform_uint() const;
 
	rng_std_mt();
	rng_std_mt(const rng_std_mt&) = delete;
	rng_std_mt& operator=(const rng_std_mt&) = delete;
	rng_std_mt(rng_std_mt &&other) = default;

	vec2 uniform_float2() const;
};

/* This class is adapted from PBRTv3, license of the original:
 * {{{
 * pbrt source code is Copyright(c) 1998-2016
 *                     Matt Pharr, Greg Humphreys, and Wenzel Jakob.
 * 
 * This file is part of pbrt.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * }}}
 *
 */
class rng_pcg {
	uint64_t state, inc;
public:
	static constexpr float FLOAT_ONE_MINUS_EPSILON = 0x1.fffffep-1;
	static constexpr uint64_t PCG32_DEFAULT_STATE = 0x853c49e6748fea9bULL;
	static constexpr uint64_t PCG32_DEFAULT_STREAM = 0xda3e39cb94b95bdbULL;
	static constexpr uint64_t PCG32_MULT = 0x5851f42d4c957f2dULL;
	
	rng_pcg() : state(PCG32_DEFAULT_STATE), inc(PCG32_DEFAULT_STREAM) {}
	rng_pcg(uint64_t initseq) { set_sequence(initseq); }
	void set_sequence(uint64_t initseq) {
		state = 0u;
		inc = (initseq << 1u) | 1u;
		uniform_uint();
		state += PCG32_DEFAULT_STATE;
		uniform_uint();
	}

	uint32_t uniform_uint() {
		uint64_t oldstate = state;
		state = oldstate * PCG32_MULT + inc;
		uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
		uint32_t rot = (uint32_t)(oldstate >> 59u);
		return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31));
	}
    uint32_t uniform_uint(uint32_t b) {
        uint32_t threshold = (~b + 1u) % b;
        while (true) {
            uint32_t r = uniform_uint();
            if (r >= threshold) return r % b;
        }
    }
    float uniform_float() {
        return std::min(FLOAT_ONE_MINUS_EPSILON, float(uniform_uint() * 0x1p-32f));
	}

	std::pair<uint64_t, uint64_t> config() const { return {state, inc}; }
};

// vim: set foldmethod=marker:
