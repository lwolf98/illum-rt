#pragma once

#include "../base.h"

#include "libgi/scene.h"

namespace wf {
	namespace cuda {
		struct simple : public batch_rt {
		public:
			simple() : bvh_nodes("bvh_nodes", 0), bvh_index("index", 0) {};
			void build(::scene *scene) override;
			void compute_hit(bool anyhit = false);

			global_memory_buffer<wf::cuda::simpleBVHNode> bvh_nodes;
			global_memory_buffer<uint32_t> bvh_index;
		};
	}
}