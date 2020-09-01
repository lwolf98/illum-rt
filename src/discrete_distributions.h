#pragma once

#include <vector>
#include <string>
#include <utility>
#include <cstdint>

class distribution_1d {
	std::vector<float> f, cdf;
	float integral_1spaced;
	void build_cdf();
public:
	distribution_1d(const std::vector<float> &f);
	distribution_1d(std::vector<float> &&f);
	std::pair<uint32_t,float> sample_index(float xi) const;
	void debug_out(const std::string &p) const;
};
