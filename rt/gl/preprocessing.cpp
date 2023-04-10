#include "preprocessing.h"

#include "libgi/color.h"

#include <iostream>
#include <fstream>
using std::cout, std::endl;

namespace wf::gl {
		
	void build_accel_struct::run() {
		pf->rt->build(pf->sd);
	}

	compute_light_distribution::compute_light_distribution()
	: f("light dist / f", 1, GL_R32F),
	  cdf("light dist / cdf", 1, GL_R32F),
	  tri_lights("light dist / tri lights", 1, GL_RGBA32I) {
	}

	void compute_light_distribution::run() {
		std::vector<float> light_power;
		std::vector<::triangle> tri_light_host;
		for (int i = 0; i < ::rc->scene.lights.size(); ++i)
			if (auto *light = dynamic_cast<trianglelight*>(::rc->scene.lights[i])) {
				tri_light_host.push_back(*(triangle*)light);
				light_power.push_back(luma(light->power()));
			}
		tri_lights.resize(tri_light_host.size(), reinterpret_cast<ivec4*>(tri_light_host.data()));

		distribution_1d dist(light_power);
// 		dist.debug_out("/tmp/foolights");

		f.resize(dist.f);
		cdf.resize(dist.cdf);
		integral_1spaced = dist.integral_1spaced;
		n = tri_light_host.size();
	}

}
