#pragma once

#include "base.h"

#include <curand_kernel.h>
#include <curand_mtgp32_kernel.h>


__global__ void setup_ray(glm::vec3 U, glm::vec3 V, float near_w, float near_h,
						  int2 resolution, float3 cam_pos, float3 cam_dir,
						  float4 *rays);
__global__ void setup_ray_incoherent(int2 resolution, float4 *rays,
									 float3 sphere1, float3 sphere2,
									 float r1, float r2,float r_max,
									 curandStateMtgp32 *rand_state);

__global__ void simple_trace(int2 resolution, float4 *rays, float4 *vertex_pos,
							 uint4 *triangles, uint32_t *index,
							 wf::cuda::simple_bvh_node *bvh_nodes,
							 wf::cuda::tri_is *intersections,
							 bool anyhit = false);

#define TRACE_PARAMETERS1 const int2 resolution, float4 *rays, cudaTextureObject_t rays_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, uint1 *index, cudaTextureObject_t index_tex, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, wf::cuda::tri_is *intersections, bool anyhit
#define TRACE_PARAMETERS2 const int num_rays,    float4 *rays, cudaTextureObject_t rays_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, uint1 *index, cudaTextureObject_t index_tex, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, wf::cuda::tri_is *intersections, bool anyhit
__global__ void ifif_trace(TRACE_PARAMETERS1);
__global__ void whilewhile_trace(TRACE_PARAMETERS1);
__global__ void speculativewhilewhile_trace(TRACE_PARAMETERS1);
__global__ void persistentwhilewhile_trace(TRACE_PARAMETERS2);
__global__ void persistentifif_trace(TRACE_PARAMETERS2);
__global__ void persistentspeculativewhilewhile_trace(TRACE_PARAMETERS2);
__global__ void dynamicwhilewhile_trace(TRACE_PARAMETERS2);

#define TRAVERSAL_PARAMETERS int &ray_index, float3 &ray_o, float3 &ray_d, float3 &ray_id, float3 &ray_ood, float tmin, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, uint1 *index, cudaTextureObject_t index_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, wf::cuda::tri_is &hit, bool anyhit
__forceinline__ __device__ void ifif_traversal(TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void whilewhile_traversal(TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void speculativewhilewhile_traversal(TRAVERSAL_PARAMETERS);

#define INTERSECT_BOX_PARAMETERS float3 &boxmin, float3 &boxmax, float3 &ray_o, float3 &ray_d, float3 &ray_id, float3 &ray_ood, const float &t_min, const float &t_max, float &hit_t
__forceinline__ __device__ bool intersect_box(INTERSECT_BOX_PARAMETERS);
__forceinline__ __device__ bool intersect_box_shirley(INTERSECT_BOX_PARAMETERS);
__forceinline__ __device__ bool intersect_box_aila(INTERSECT_BOX_PARAMETERS);

#define INTERSECT_TRIANGLE_PARAMETERS const float3 &v1, const float3 &v2, const float3 &v3, const float3 &ray_o, const float3 &ray_d, const float t_min, const float t_max, float &hit_t, float &hit_beta, float &hit_gamma
__forceinline__ __device__ bool intersect_triangle(INTERSECT_TRIANGLE_PARAMETERS);
__forceinline__ __device__ bool intersect_triangle_shirley(INTERSECT_TRIANGLE_PARAMETERS);
__forceinline__ __device__ bool intersect_triangle_moeller_trumbore(INTERSECT_TRIANGLE_PARAMETERS);

__forceinline__ __device__ __host__ float dot(const float3 &a, const float3 &b);
__forceinline__ __device__ __host__ float3 cross(const float3 &a, const float3 &b);
__forceinline__ __device__ __host__ void cross(float3 &dest, const float3 &a, const float3 &b);
__forceinline__ __device__ void normalize(float3 &v);

__forceinline__ __device__ int vmin_max (int a, int b, int c);
__forceinline__ __device__ int vmax_min (int a, int b, int c);
__forceinline__ __device__ int vmin_min (int a, int b, int c);
__forceinline__ __device__ int vmax_max (int a, int b, int c);
