#include "preprocessing.h"

#include "libgi/util.h"

#include <algorithm>

namespace wf::cpu {
		
	void build_accel_struct::run() {
		pf->rt->build(pf->sd);
	}

	void compute_light_distribution::run() {
		unsigned prims = 0;
		
		// TODO
	}

}
