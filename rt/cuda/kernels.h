#pragma once

#include "base.h"

#ifdef __CUDACC__

#include <curand_kernel.h>
#include <curand_mtgp32_kernel.h>

__global__ void simple_trace(int2 resolution, float4 *rays, float4 *vertex_pos,
							 uint4 *triangles, uint32_t *index,
							 wf::cuda::simple_bvh_node *bvh_nodes,
							 wf::cuda::tri_is *intersections,
							 bool anyhit = false);

__global__ void simple_trace_alpha(int2 resolution, float4 *rays, float4 *vertex_pos,
                                   uint4 *triangles, uint32_t *index,
                                   wf::cuda::simple_bvh_node *bvh_nodes,
                                   wf::cuda::tri_is *intersections,
                                   wf::cuda::material *materials,
                                   float2 *tex_coords,
                                   bool anyhit);

#define TRACE_PARAMETERS1 const int2 resolution, float4 *rays, cudaTextureObject_t rays_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, uint *index, cudaTextureObject_t index_tex, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, wf::cuda::tri_is *intersections, bool anyhit
#define TRACE_PARAMETERS2 const int num_rays,    float4 *rays, cudaTextureObject_t rays_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, uint *index, cudaTextureObject_t index_tex, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, wf::cuda::tri_is *intersections, bool anyhit
#define ALPHA_TRACE_PARAMETERS1 const int2 resolution, float4 *rays, cudaTextureObject_t rays_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, uint *index, cudaTextureObject_t index_tex, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, wf::cuda::tri_is *intersections, wf::cuda::material *materials, float2 *tex_coords, bool anyhit
#define ALPHA_TRACE_PARAMETERS2 const int num_rays,    float4 *rays, cudaTextureObject_t rays_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, uint *index, cudaTextureObject_t index_tex, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, wf::cuda::tri_is *intersections, wf::cuda::material *materials, float2 *tex_coords, bool anyhit

__global__ void ifif_trace(TRACE_PARAMETERS1);
__global__ void ifif_trace_alpha(ALPHA_TRACE_PARAMETERS1);
__global__ void whilewhile_trace(TRACE_PARAMETERS1);
__global__ void whilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS1);
__global__ void speculativewhilewhile_trace(TRACE_PARAMETERS1);
__global__ void speculativewhilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS1);
__global__ void persistentwhilewhile_trace(TRACE_PARAMETERS2);
__global__ void persistentwhilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS2);
__global__ void persistentifif_trace(TRACE_PARAMETERS2);
__global__ void persistentifif_trace_alpha(ALPHA_TRACE_PARAMETERS2);
__global__ void persistentspeculativewhilewhile_trace(TRACE_PARAMETERS2);
__global__ void persistentspeculativewhilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS2);
__global__ void dynamicwhilewhile_trace(TRACE_PARAMETERS2);
__global__ void dynamicwhilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS2);

#endif


void launch_initialize_framebuffer_data(int2 resolution, float4 *framebuffer);

void launch_setup_rays(glm::vec3 U, glm::vec3 V, float near_w, float near_h, int2 resolution, 
					   float3 cam_pos, float3 cam_dir, float2 *random_numbers, float4 *rays);

void launch_setup_ray_incoherent(int2 config, int2 resolution, float4 *rays,
								 float3 sphere1, float3 sphere2, float r1, float r2,float r_max,
								 curandStateMtgp32 *rand_state);

void launch_add_hitpoint_albedo(int2 res, wf::cuda::tri_is *intersections, 
								uint4 *triangles, float2 *tex_coords, wf::cuda::material *materials, float4 *framebuffer);




