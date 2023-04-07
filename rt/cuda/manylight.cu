#include "manylight.h"

#include "libgi/util.h"
#include "libgi/sampling.h"

#include "cuda-operators.h"

#define launch_config NUM_BLOCKS_FOR_RESOLUTION(res), DESIRED_BLOCK_SIZE
namespace wf::cuda {
	/* frame preparation */

	/*namespace k {
		/*static __device__ bool not_black(float4 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}* /
		static __global__ void sample_v_0s(int2 res, float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
										   uint4 *triangles, float4 *vert_norm, material *materials,
										   float *pdf, float2 *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;
		}
	}

	void sample_v_0s::run() {
		rng.compute();
		int2 res = frame_res();
		k::sample_v_0s<<<launch_config>>>(res,
										  camdata->rays.device_memory,
										  camdata->intersections.device_memory,
										  bouncedata->rays.device_memory,
										  camdata->framebuffer.device_memory,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->materials.device_memory,
										  pdf->data.device_memory,
										  rng.random_numbers);
	}*/

	/* integration */

	namespace k {
		/*static __device__ bool not_black(float4 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}*/
		static __global__ void integrate_vpl_samples(int2 res, float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
										   uint4 *triangles, float4 *vert_norm, material *materials,
										   vpl *sampled_vpls) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			float3 radiance {0,1.f,0};
			framebuffer[ray_index] = framebuffer[ray_index] + make_float4(radiance.x, radiance.y, radiance.z, 1.0);
		}
	}

	//TODO: use util functions centralized; (frame_res copied from bounce.cu)
	int2 my_frame_res() { auto r = rc->resolution(); return {r.x,r.y}; }

	void integrate_vpl_samples::run() {
		int2 res = my_frame_res();
		k::integrate_vpl_samples<<<launch_config>>>(res,
										  camrays->rays.device_memory,
										  camrays->intersections.device_memory,
										  shadowrays->rays.device_memory,
										  camrays->framebuffer.device_memory,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->materials.device_memory,
										  sampled_vpls);
	}

}