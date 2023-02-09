#include "manylight.h"

#include "libgi/util.h"
//#include "libgi/sampling.h"

#include <iostream>
using namespace std;

namespace wf::cpu {
	void manylight_step::run() {
		time_this_wf_step;
		auto res = rc->resolution();
		#pragma omp parallel for
		for (int y = 0; y < res.y; ++y) {
			for (int x = 0; x < res.x; ++x) {
				/*if (x%20==0||y%20==0) {
					vec3 radiance = vec3(1, 0, 0);
					//rc->framebuffer.color(x,y) += vec4(radiance, 1);
					rc->framebuffer.color(x,y) = vec4(radiance, 1);
				}*/
				rc->framebuffer.color(x,y) += vec4(vec3(0,0,0.2), 1);
			}
		}
	}
}