#include "preprocessing.h"

namespace wf::cpu {
		
	void build_accel_struct::run() {
		pf->rt->build(pf->sd);
	}

}
