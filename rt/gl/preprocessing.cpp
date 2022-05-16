#include "preprocessing.h"

namespace wf::gl {
		
	void build_accel_struct::run() {
		pf->rt->build(pf->sd);
	}

}
