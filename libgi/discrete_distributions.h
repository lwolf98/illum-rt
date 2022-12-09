#pragma once

#include "rt.h"

#include <vector>
#include <string>
#include <utility>
#include <cstdint>

class distribution_1d {
	// f holds the discrete function values that the distribution will be built on
	// cdf holds the cdf of the pdf derived from f
	std::vector<float> f, cdf;
	// integral_1spaced is the sum of all entries in f
	// it is called 1spaced as each sample of f is one unit apart
	// i.e. it is the integral over f (and, consequently, the pdf's scaling factor)
	float integral_1spaced;
	void build_cdf();

public:
	distribution_1d(const std::vector<float> &f);
	distribution_1d(std::vector<float> &&f);
	pair<uint32_t,float> sample_index(float xi) const;
	float pdf(uint32_t index) const;
	void debug_out(const std::string &p) const;
	float integral() const { return integral_1spaced; }

	float value_at(int index) const { return f[index]; }
	int size() const { return f.size(); }
	
#ifndef RTGI_SKIP_SKY
	struct linearly_interpolated_01 {
		distribution_1d &discrete;
		linearly_interpolated_01(distribution_1d &discrete) : discrete(discrete) {}

		pair<float,float> sample(float xi) const;
		float pdf(float t) const;
		float integral() const { return discrete.integral_1spaced / discrete.f.size(); }
	}
	linearly_interpolated_on_01;
	friend linearly_interpolated_01;
#endif

};

#ifndef RTGI_SKIP_SKY
class distribution_2d {
	const float *f;
	int w, h;

	std::vector<distribution_1d> conditional;
	distribution_1d *marginal = nullptr;

	void build_cdf();
public:
	distribution_2d(const float *f, int w, int h);
	pair<vec2,float> sample(vec2 xi) const;
	float pdf(vec2 sample) const;
// 	void debug_out(const std::string &p) const;
	float integral() const { assert(marginal); return marginal->integral(); }
	float unit_integral() const { assert(marginal); return marginal->integral() / (w*h); }
	void debug_out(const std::string &p, int n) const;
};
#endif
