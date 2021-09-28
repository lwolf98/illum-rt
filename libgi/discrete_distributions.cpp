#include "discrete_distributions.h"

#include "color.h"
#include "random.h"
#include "framebuffer.h"

#include <png++/png.hpp>

#include <cassert>
#include <fstream>

using namespace std;;

distribution_1d::distribution_1d(const std::vector<float> &f)
#ifndef RTGI_SKIP_SKY
: f(f), cdf(f.size()+1), linearly_interpolated_on_01(*this) {
#else
: f(f), cdf(f.size()+1) {
#endif
	build_cdf();
}

distribution_1d::distribution_1d(std::vector<float> &&f)
#ifndef RTGI_SKIP_SKY
: f(f), cdf(f.size()+1), linearly_interpolated_on_01(*this) {
#else
: f(f), cdf(f.size()+1) {
#endif
	build_cdf();
}

void distribution_1d::build_cdf() {
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING_IMPL
	cdf[0] = 0;
	unsigned N = f.size();
	for (unsigned i = 1; i < N+1; ++i)
		cdf[i] = cdf[i-1] + f[i-1];
	integral_1spaced = cdf[N];
	for (unsigned i = 1; i < N+1; ++i)
		cdf[i] = integral_1spaced > 0 ? cdf[i]/integral_1spaced : i/float(N);
	assert(cdf[N] == 1.0f);
#else
	// todo: set up cdf table for given discrete pdf (stored in f)
#endif
}

pair<uint32_t,float> distribution_1d::sample_index(float xi) const {
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING_IMPL
	unsigned index = unsigned(lower_bound(cdf.begin(), cdf.end(), xi) - cdf.begin());
	index = index > 0 ? index - 1 : index; // might happen for xi==0
	float pdf = integral_1spaced > 0.0f ? f[index] / integral_1spaced : 0.0f;
	return pair{index,pdf};;
#else
	// todo: find index corresponding to xi, also return the proper PDF value for that index
	return {0,1.0f};
#endif
}

float distribution_1d::pdf(uint32_t index) const {
	return integral_1spaced > 0.0f ? f[index] / integral_1spaced : 0.0f;
}

void distribution_1d::debug_out(const std::string &p) const {
	ofstream pdf(p+".pdf");
	for (auto x : f)
		pdf << x / integral_1spaced << endl;
	pdf.close();

	ofstream cdf(p+".cdf");
	for (auto x : this->cdf)
		cdf << x << endl;
	cdf.close();
}

#ifndef RTGI_SKIP_SKY
pair<float,float> distribution_1d::linearly_interpolated_01::sample(float xi) const {
	unsigned index = unsigned(lower_bound(discrete.cdf.begin(), discrete.cdf.end(), xi) - discrete.cdf.begin());
	index = index > 0 ? index - 1 : index; // might happen for xi==0
	float du = xi - discrete.cdf[index];
	if (discrete.cdf[index+1] - discrete.cdf[index] > 0)
		du /= discrete.cdf[index+1]-discrete.cdf[index];
	float pdf = discrete.integral_1spaced > 0 ? discrete.f[index] / integral() : 0.0f;
	return { (index+du) / discrete.size(), pdf };
}

float distribution_1d::linearly_interpolated_01::pdf(float t) const {
	assert(t >= 0 && t<1.0f);
	return discrete.f[t*discrete.f.size()] / integral();
}
#endif

#ifndef RTGI_SKIP_SKY

distribution_2d::distribution_2d(const float *f, int w, int h) : f(f), w(w), h(h) {
	conditional.reserve(h);
	for (int y = 0; y < h; ++y) {
		vector<float> v(f+y*w, f+y*w+w);
		conditional.emplace_back(std::move(v));
	}
	std::vector<float> marginal_f(h);
	for (int y = 0; y < h; ++y)
		marginal_f[y] = conditional[y].integral();
	marginal = new distribution_1d(std::move(marginal_f));
}

pair<vec2,float> distribution_2d::sample(vec2 xi) const {
	auto [y, marg_pdf] = marginal->linearly_interpolated_on_01.sample(xi.x);
	auto [x, cond_pdf] = conditional[int(y * marginal->size())].linearly_interpolated_on_01.sample(xi.y);
	assert(std::isfinite(marg_pdf));
	assert(std::isfinite(cond_pdf));
	return { vec2(x,y), marg_pdf*cond_pdf };
}

float distribution_2d::pdf(vec2 sample) const {
	assert(sample.x >= 0 && sample.x <= 1); // TODO <= ??
	assert(sample.y >= 0 && sample.y <= 1);
	return conditional[int(sample.y*h)].value_at(int(sample.x*w)) / marginal->integral();
}

void distribution_2d::debug_out(const std::string &p, int n) const {
	using namespace png;
	image<rgb_pixel> out(w, h);
	buffer<int> counts(w, h);
	counts.clear(0);
	rng rng;
	int max = 0;
	for (int i = 0; i < n; ++i) {
		auto [sample,pdf] = this->sample(rng.uniform_float2());
		int x = sample.x*w;
		int y = sample.y*h;
		assert(x >= 0 && x < w);
		assert(y >= 0 && y < h);
		max = fmaxf(++counts(x,y), max);
	}
	counts.for_each([&](int x, int y) {
						float c = (float)counts(x,y) / max;
						vec3 col = heatmap(c);
						out[y][x] = rgb_pixel(col.x*255, col.y*255, col.z*255);
					});
	out.write(p);
}
#endif
