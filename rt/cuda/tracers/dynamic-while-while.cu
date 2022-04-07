#include "dynamic-while-while.h"

#include "libgi/timer.h"

#include <iostream>

#include "kernels.h"
#include "cuda-helpers.h"

namespace wf{
	namespace cuda{
		void dynamicwhilewhile::compute_hit(bool anyhit) {
			auto *rt = dynamic_cast<wf::cuda::dynamicwhilewhile*>(rc->scene.batch_rt);
			assert(rt != nullptr);

			// dynamicwhilewhileTrace<<<DESIRED_BLOCKS_COUNT, DESIRED_BLOCK_SIZE>>>(
			// dynamicwhilewhile-Kernel uses 48 Registers instead of 40, so run one less warp than usual for best occupation
			dynamicwhilewhileTrace<<<DESIRED_BLOCKS_COUNT, dim3(WARPSIZE, DESIRED_WARPS_PER_BLOCK-1, 1)>>>(
														    rc->resolution().x * rc->resolution().y,
															rd->rays.device_memory,
															rd->rays.tex,
															sd->vertex_pos.device_memory,
															sd->vertex_pos.tex,
															sd->triangles.device_memory,
															sd->triangles.tex,
															rt->bvh_index.device_memory,
															rt->bvh_index.tex,
															rt->bvh_nodes.device_memory,
															rt->bvh_nodes.tex,
															rd->intersections.device_memory,
															anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError());
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize());
		}
	}
}
