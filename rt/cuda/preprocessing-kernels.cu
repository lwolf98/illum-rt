#include "base.h"
#include "preprocessing.h"

#include <glm/gtx/transform.hpp>

// stored as rows for efficient M*v
struct mat3 {
	float a[9];
	__device__ float& at(int x, int y) {
		return a[y*3+x];
	}
	__device__ float4 operator*(const float4 &f) {
		float4 res;
		res.x = f.x * at(0,0) + f.y * at(1,0) + f.z * at(2,0);
		res.y = f.x * at(0,1) + f.y * at(1,1) + f.z * at(2,1);
		res.z = f.x * at(0,2) + f.y * at(1,2) + f.z * at(2,2);
		res.w = 1.0;
		return res;
	}
};

__global__ void rotate_scene(mat3 rot, float4 *vertex_pos_dst, const float4 *vertex_pos_src, float4 *vertex_norm, int vertices) {
	int id = blockIdx.x * blockDim.x + threadIdx.x;
	if (id >= vertices) return;

	float4 p = vertex_pos_src[id];
	vertex_pos_dst[id] = rot*p;
}


namespace wf::cuda::k {

	void rotate_scene(const glm::mat4 &rot, float4 *vertex_pos_dst, const float4 *vertex_pos_src, float4 *vertex_norm, int vertices) {
		int threads = DESIRED_THREADS_PER_BLOCK;
		int blocks = (vertices+DESIRED_THREADS_PER_BLOCK-1)/DESIRED_THREADS_PER_BLOCK;
		mat3 m { rot[0][0], rot[1][0], rot[2][0],
		         rot[0][1], rot[1][1], rot[2][1],
		         rot[0][2], rot[1][2], rot[2][2] };
		::rotate_scene<<<blocks,threads>>>(m, vertex_pos_dst, vertex_pos_src, vertex_norm, vertices);
	}

}
