#pragma once

#include "libgi/subdivision.h"
#include "cuda-helpers.h"

namespace wf {
	namespace cuda {
		struct aabb_f3 {
			float3 min;
			float3 max;
		};

#if !defined(SLAB_COMPRESSION) && !defined(QUANTIZATION)
		typedef subd::patch_slab_node<float4> patch_node;
#else
		typedef subd::patch_slab_node patch_node;
#endif

	}
}