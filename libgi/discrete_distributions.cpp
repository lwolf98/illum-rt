#include "discrete_distributions.h"

#include <cassert>
#include <fstream>

using namespace std;;

distribution_1d::distribution_1d(const std::vector<float> &f) : f(f), cdf(f.size()+1) {
	build_cdf();
}

distribution_1d::distribution_1d(std::vector<float> &&f) : f(std::move(f)), cdf(f.size()+1) {
	build_cdf();
}

void distribution_1d::build_cdf() {
	cdf[0] = 0;
	unsigned N = f.size();
	for (unsigned i = 1; i < N+1; ++i)
		cdf[i] = cdf[i-1] + f[i-1];
	integral_1spaced = cdf[N];
	for (unsigned i = 1; i < N+1; ++i)
		cdf[i] = integral_1spaced > 0 ? cdf[i]/integral_1spaced : i/float(N);
	assert(cdf[N] == 1.0f);
}

pair<uint32_t,float> distribution_1d::sample_index(float xi) const {
	unsigned index = unsigned(lower_bound(cdf.begin(), cdf.end(), xi) - cdf.begin());
	index = index > 0 ? index - 1 : index; // might happen for xi==0
	float pdf = integral_1spaced > 0.0f ? f[index] / integral_1spaced : 0.0f;
	return pair{index,pdf};;
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
