#pragma once

#include "../base.h"

#include "curand_kernel.h"
#include "curand_mtgp32_kernel.h"


__global__ void setupRay(glm::vec3 U, glm::vec3 V, float near_w, float near_h,
								 int2 resolution, float3 camPos, float3 camDir,
								 float4* rays);
__global__ void setupRayIncoherent(int2 resolution, float4* rays,
                                   float3 sphere1, float3 sphere2,
                                   float r1, float r2,float r_max,
                                   curandStateMtgp32* rand_state);

__global__ void simpleTrace(int2 resolution, float4* rays, float4* vertex_pos,
                            uint4* triangles, uint32_t* index,
                            wf::cuda::simpleBVHNode* bvh_nodes,
                            wf::cuda::tri_is* intersections,
                            bool anyhit = false);

#define TRACE_PARAMETERS1 const int2 resolution, float4* rays, cudaTextureObject_t rays_tex, float4* vertex_pos, cudaTextureObject_t vertex_pos_tex, uint4* triangles, cudaTextureObject_t triangles_tex, uint1* index, cudaTextureObject_t index_tex, wf::cuda::compactBVHNode* bvh_nodes, cudaTextureObject_t bvh_nodes_tex, wf::cuda::tri_is* intersections, bool anyhit
#define TRACE_PARAMETERS2 const int numRays,     float4* rays, cudaTextureObject_t rays_tex, float4* vertex_pos, cudaTextureObject_t vertex_pos_tex, uint4* triangles, cudaTextureObject_t triangles_tex, uint1* index, cudaTextureObject_t index_tex, wf::cuda::compactBVHNode* bvh_nodes, cudaTextureObject_t bvh_nodes_tex, wf::cuda::tri_is* intersections, bool anyhit
__global__ void ififTrace(TRACE_PARAMETERS1);
__global__ void whilewhileTrace(TRACE_PARAMETERS1);
__global__ void speculativewhilewhileTrace(TRACE_PARAMETERS1);
__global__ void persistentwhilewhileTrace(TRACE_PARAMETERS2);
__global__ void persistentififTrace(TRACE_PARAMETERS2);
__global__ void persistentspeculativewhilewhileTrace(TRACE_PARAMETERS2);
__global__ void dynamicwhilewhileTrace(TRACE_PARAMETERS2);

#define TRAVERSAL_PARAMETERS int &ray_index, float3 &ray_o, float3 &ray_d, float3 &ray_id, float3 &ray_ood, float tmin, wf::cuda::compactBVHNode* bvh_nodes, cudaTextureObject_t bvh_nodes_tex, uint1* index, cudaTextureObject_t index_tex, uint4* triangles, cudaTextureObject_t triangles_tex, float4* vertex_pos, cudaTextureObject_t vertex_pos_tex, wf::cuda::tri_is& hit, bool anyhit
__forceinline__ __device__ void ififTraversal(TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void whilewhileTraversal(TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void speculativewhilewhileTraversal(TRAVERSAL_PARAMETERS);

#define INTERSECT_BOX_PARAMETERS float3& boxmin, float3& boxmax, float3& ray_o, float3& ray_d, float3& ray_id, float3& ray_ood, const float& t_min, const float& t_max, float& hitT
__forceinline__ __device__ bool intersectBox(INTERSECT_BOX_PARAMETERS);
__forceinline__ __device__ bool intersectBoxShirley(INTERSECT_BOX_PARAMETERS);
__forceinline__ __device__ bool intersectBoxAila(INTERSECT_BOX_PARAMETERS);

#define INTERSECT_TRIANGLE_PARAMETERS const float3& v1, const float3& v2, const float3& v3, const float3& ray_o, const float3& ray_d, const float t_min, const float t_max, float &hitT, float &hitBeta, float &hitGamma
__forceinline__ __device__ bool intersectTriangle(INTERSECT_TRIANGLE_PARAMETERS);
__forceinline__ __device__ bool intersectTriangleShirley(INTERSECT_TRIANGLE_PARAMETERS);
__forceinline__ __device__ bool intersectTriangleMoellerTrumbore(INTERSECT_TRIANGLE_PARAMETERS);

__forceinline__ __device__ __host__ float dot(const float3 &a, const float3 &b);
__forceinline__ __device__ __host__ float3 cross(const float3 &a, const float3 &b);
__forceinline__ __device__ __host__ void cross(float3 &dest, const float3 &a, const float3 &b);
__forceinline__ __device__ void normalize(float3 &v);

__forceinline__ __device__ int vmin_max (int a, int b, int c);
__forceinline__ __device__ int vmax_min (int a, int b, int c);
__forceinline__ __device__ int vmin_min (int a, int b, int c);
__forceinline__ __device__ int vmax_max (int a, int b, int c);