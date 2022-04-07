#pragma once

#include "../base.h"

#include "libgi/scene.h"

namespace wf {
	namespace cuda {
		struct ifif : public batch_rt {
		public:
			void compute_hit(bool anyhit = false);
		};
	}
}