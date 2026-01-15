#include "base.h"
#include "preprocessing.h"

#include <glm/gtx/transform.hpp>

__global__ void rotate_scene(wf::cuda::mat3 rot, float4 *vertex_pos_dst, const float4 *vertex_pos_src, float4 *vertex_norm, int vertices) {
	for (int vertex_id = blockIdx.x * blockDim.x + threadIdx.x; vertex_id < vertices; vertex_id += blockDim.x * gridDim.x) {
		float4 p = vertex_pos_src[vertex_id];
		vertex_pos_dst[vertex_id] = rot*p;
	}
}

namespace wf::cuda::k {

	void rotate_scene(const glm::mat4 &rot, float4 *vertex_pos_dst, const float4 *vertex_pos_src, float4 *vertex_norm, int vertices) {
		int threads = DESIRED_THREADS_PER_BLOCK;
		int blocks = DESIRED_BLOCKS_COUNT;
		wf::cuda::mat3 m { rot[0][0], rot[1][0], rot[2][0],
		         rot[0][1], rot[1][1], rot[2][1],
		         rot[0][2], rot[1][2], rot[2][2] };
		::rotate_scene<<<blocks,threads>>>(m, vertex_pos_dst, vertex_pos_src, vertex_norm, vertices);
	}

}
