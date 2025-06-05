#include "kernels.h"
#include "cuda-operators.h"
#include "trace-helper.cuh"
// -----------------------------------------------------------------------------
// Global constants
#define STACK_SIZE 64 // Traversal Stack size for all methods
#define BATCH_SIZE 96 // persistent methods: Number of rays to fetch at a time, must be multiple of 32
#define MAX_BLOCK_HEIGHT 10 // Upper bound for blockDim.y. RTX 3090 has 84 SMs -> ceil(sqrt(84))= 10. Will have to be adjusted up for future bigger GPUs
#define DYNAMIC_FETCH_THRESHOLD 20 // If fewer than this active, fetch new rays
#define SENTINEL 0x76543210 // stack sentinel
// -----------------------------------------------------------------------------
// Configures whether Nodes, Rays etc. are loaded through texture pipeline or global memory
// if you change any of these, do a 'make clean', it doesn't work without it for some reason
#define FETCH_NODE(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)
#define FETCH_RAY(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)
#define FETCH_TRI(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)
#define FETCH_INDEX(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)
#define FETCH_VERTEX(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)

// #define FETCH_NODE(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)
// #define FETCH_RAY(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)
// #define FETCH_TRI(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)
// #define FETCH_INDEX(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)
// #define FETCH_VERTEX(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)

#define FETCH_GLOBAL(NAME, IDX, TYPE) ((const TYPE*)NAME)[IDX]
#define FETCH_TEXTURE(NAME, IDX, TYPE) tex1Dfetch<TYPE>(NAME##_tex, IDX)
// -----------------------------------------------------------------------------


#define TRAVERSAL_PARAMETERS int &ray_index, float3 &ray_o, float3 &ray_d, float3 &ray_id, float3 &ray_ood, float ray_tmin, float ray_tmax, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, uint *index, cudaTextureObject_t index_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, wf::cuda::tri_is &hit, bool anyhit
#define ALPHA_TRAVERSAL_PARAMETERS int &ray_index, float3 &ray_o, float3 &ray_d, float3 &ray_id, float3 &ray_ood, float ray_tmin, float ray_tmax, wf::cuda::compact_bvh_node *bvh_nodes, cudaTextureObject_t bvh_nodes_tex, uint *index, cudaTextureObject_t index_tex, uint4 *triangles, cudaTextureObject_t triangles_tex, float4 *vertex_pos, cudaTextureObject_t vertex_pos_tex, wf::cuda::tri_is &hit, wf::cuda::material *materials, float2 *tex_coords, bool anyhit

__forceinline__ __device__ void ifif_traversal(TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void ifif_traversal_alpha(ALPHA_TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void whilewhile_traversal(TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void whilewhile_traversal_alpha(ALPHA_TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void speculativewhilewhile_traversal(TRAVERSAL_PARAMETERS);
__forceinline__ __device__ void speculativewhilewhile_traversal_alpha(ALPHA_TRAVERSAL_PARAMETERS);

__device__ int global_counter; // used in persistent methods

constexpr const float ALPHA_TRESHOLD = 0.5f;

__forceinline__ __device__ bool is_below_alpha_threshold(const wf::cuda::tri_is &intersection, 
                                                         const uint4 &tri,
                                                         const wf::cuda::material *materials,
                                                         const float2 *tex_coords) {
	wf::cuda::material m = materials[tri.w];
	if (m.albedo_tex > 0) {
		float2 tc = (1.0f - intersection.beta - intersection.gamma) * tex_coords[tri.x]
		            + intersection.beta * tex_coords[tri.y] 
		            + intersection.gamma * tex_coords[tri.z];
		float4 tex = tex2D<float4>(m.albedo_tex, tc.x, tc.y);
		if (tex.w < ALPHA_TRESHOLD)
			return true;
	}
	return false;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void simple_trace(int2 resolution, float4 *rays, float4 *vertex_pos,
							 uint4 *triangles, uint32_t *index,
							 wf::cuda::simple_bvh_node *bvh_nodes,
							 wf::cuda::tri_is *intersections,
							 bool anyhit) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if (i >= resolution.x || j >= resolution.y)
        return;

    float4 ray_o4 = FETCH_GLOBAL(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_GLOBAL(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
	float ray_tmin = ray_o4.w;
	float ray_tmax = ray_d4.w;
    float3 ray_id;
    const float ooeps = exp2f(-80.f); // avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    wf::cuda::tri_is intersection;

    uint32_t stack[25];
    int32_t stack_pointer = 0;
    stack[0] = 0;
    while (stack_pointer >= 0) {
        wf::cuda::simple_bvh_node node = bvh_nodes[stack[stack_pointer--]];
        if (node.inner()) {
            // Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersect_box(node.box_l_min, node.box_l_max, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, l_tmin);
            const bool hit_r = intersect_box(node.box_r_min, node.box_r_max, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, r_tmin);

            if (hit_l && hit_r) {
                if (l_tmin < r_tmin) {
                    stack[++stack_pointer] = node.link_r;
                    stack[++stack_pointer] = node.link_l;
                }
                else {
                    stack[++stack_pointer] = node.link_l;
                    stack[++stack_pointer] = node.link_r;
                }
            }
            else if (hit_l)
                stack[++stack_pointer] = node.link_l;
            else if (hit_r)
                stack[++stack_pointer] = node.link_r;
        }
        else {
            for (int t = 0; t < node.tri_count(); ++t) {
                int tri_index = index[node.tri_offset()+t];
                uint4 tri = triangles[tri_index];

                float3 v1 = make_float3(vertex_pos[tri.x].x, vertex_pos[tri.x].y, vertex_pos[tri.x].z);
                float3 v2 = make_float3(vertex_pos[tri.y].x, vertex_pos[tri.y].y, vertex_pos[tri.y].z);
                float3 v3 = make_float3(vertex_pos[tri.z].x, vertex_pos[tri.z].y, vertex_pos[tri.z].z);

                if (intersect_triangle(v1, v2, v3,
                                        ray_o, ray_d,
                                        ray_tmin, ray_tmax,
                                        intersection.t, intersection.beta, intersection.gamma)) {
                    if (anyhit) { // trace any hit
                        intersections[ray_index] = intersection;
                        return;
                    }
                    else // trace closest hit
                        if (intersection.t < closest.t) {
                            closest = intersection;
                            //closest.ref = tri_index;
                            closest.set_ref(tri_index);
                        }
                }
            }
        }
    }
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void simple_trace_alpha(int2 resolution, float4 *rays, float4 *vertex_pos,
                                   uint4 *triangles, uint32_t *index,
                                   wf::cuda::simple_bvh_node *bvh_nodes,
                                   wf::cuda::tri_is *intersections,
                                   wf::cuda::material *materials,
                                   float2 *tex_coords,
                                   bool anyhit) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if (i >= resolution.x || j >= resolution.y)
        return;

    float4 ray_o4 = FETCH_GLOBAL(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_GLOBAL(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
	float ray_tmin = ray_o4.w;
	float ray_tmax = ray_d4.w;
    float3 ray_id;
    const float ooeps = exp2f(-80.f); // avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    wf::cuda::tri_is intersection;

    uint32_t stack[25];
    int32_t stack_pointer = 0;
    stack[0] = 0;
    while (stack_pointer >= 0) {
        wf::cuda::simple_bvh_node node = bvh_nodes[stack[stack_pointer--]];
        if (node.inner()) {
            // Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersect_box(node.box_l_min, node.box_l_max, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, l_tmin);
            const bool hit_r = intersect_box(node.box_r_min, node.box_r_max, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, r_tmin);

            if (hit_l && hit_r) {
                if (l_tmin < r_tmin) {
                    stack[++stack_pointer] = node.link_r;
                    stack[++stack_pointer] = node.link_l;
                }
                else {
                    stack[++stack_pointer] = node.link_l;
                    stack[++stack_pointer] = node.link_r;
                }
            }
            else if (hit_l)
                stack[++stack_pointer] = node.link_l;
            else if (hit_r)
                stack[++stack_pointer] = node.link_r;
        }
        else {
            for (int t = 0; t < node.tri_count(); ++t) {
                int tri_index = index[node.tri_offset()+t];
                uint4 tri = triangles[tri_index];

                float3 v1 = make_float3(vertex_pos[tri.x].x, vertex_pos[tri.x].y, vertex_pos[tri.x].z);
                float3 v2 = make_float3(vertex_pos[tri.y].x, vertex_pos[tri.y].y, vertex_pos[tri.y].z);
                float3 v3 = make_float3(vertex_pos[tri.z].x, vertex_pos[tri.z].y, vertex_pos[tri.z].z);

                if (intersect_triangle(v1, v2, v3,
                                        ray_o, ray_d,
                                        ray_tmin, ray_tmax,
                                        intersection.t, intersection.beta, intersection.gamma)) {
                    if (!is_below_alpha_threshold(intersection, tri, materials, tex_coords))
                        if (anyhit) { // trace any hit
                            intersections[ray_index] = intersection;
                            return;
                        }
                        else // trace closest hit
                            if (intersection.t < closest.t) {
                                closest = intersection;
                                //closest.ref = tri_index;
                            	closest.set_ref(tri_index);
                            }
                }
            }
        }
    }
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void ifif_trace(TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if (i >= resolution.x || j >= resolution.y)
        return;

    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
	float ray_tmin = ray_o4.w;
	float ray_tmax = ray_d4.w;
    float3 ray_id;
    const float ooeps = exp2f(-80.f); // avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    ifif_traversal(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void ifif_trace_alpha(ALPHA_TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if (i >= resolution.x || j >= resolution.y)
        return;

    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
	float ray_tmin = ray_o4.w;
	float ray_tmax = ray_d4.w;
    float3 ray_id;
    const float ooeps = exp2f(-80.f); // avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    ifif_traversal_alpha(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, materials, tex_coords, anyhit);
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK, DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void whilewhile_trace(TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if (i >= resolution.x || j >= resolution.y)
        return;

    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
	float ray_tmin = ray_o4.w;
	float ray_tmax = ray_d4.w;
    float3 ray_id;
    const float ooeps = exp2f(-80.f); // avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    whilewhile_traversal(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK, DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void whilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if (i >= resolution.x || j >= resolution.y)
        return;

    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
	float ray_tmin = ray_o4.w;
	float ray_tmax = ray_d4.w;
    float3 ray_id;
    const float ooeps = exp2f(-80.f); // avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    whilewhile_traversal_alpha(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, materials, tex_coords, anyhit);
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentifif_trace(TRACE_PARAMETERS2) {
    global_counter = 0;
    __shared__ volatile int next_ray_array[MAX_BLOCK_HEIGHT];
    __shared__ volatile int ray_count_array[MAX_BLOCK_HEIGHT];
    next_ray_array[threadIdx.y] = 0;
    ray_count_array[threadIdx.y] = 0;

    volatile int &local_pool_next_ray = next_ray_array[threadIdx.y];
    volatile int &local_pool_ray_count = ray_count_array[threadIdx.y];

    while (true) {
        // Rays von globalem in lokalen Pool holen
        if (local_pool_ray_count<=0 && threadIdx.x ==0) {
            local_pool_next_ray = atomicAdd(&global_counter, BATCH_SIZE);
            local_pool_ray_count = BATCH_SIZE;
        }
        __syncwarp();
        // 32 Rays von globalem Pool holen
        int ray_index = local_pool_next_ray + threadIdx.x;
        if (ray_index >= num_rays)
            return;
        if (threadIdx.x == 0) {
            local_pool_next_ray += 32;
            local_pool_ray_count -= 32;
        }

        // ray holen
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
		float ray_tmin = ray_o4.w;
		float ray_tmax = ray_d4.w;
        float3 ray_id;
        const float ooeps = exp2f(-80.f); // avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        // traverse
        wf::cuda::tri_is closest;
        ifif_traversal(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
        // Ergebnis speichern
        intersections[ray_index] = closest;
    }
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentifif_trace_alpha(ALPHA_TRACE_PARAMETERS2) {
    global_counter = 0;
    __shared__ volatile int next_ray_array[MAX_BLOCK_HEIGHT];
    __shared__ volatile int ray_count_array[MAX_BLOCK_HEIGHT];
    next_ray_array[threadIdx.y] = 0;
    ray_count_array[threadIdx.y] = 0;

    volatile int &local_pool_next_ray = next_ray_array[threadIdx.y];
    volatile int &local_pool_ray_count = ray_count_array[threadIdx.y];

    while (true) {
        // Rays von globalem in lokalen Pool holen
        if (local_pool_ray_count<=0 && threadIdx.x ==0) {
            local_pool_next_ray = atomicAdd(&global_counter, BATCH_SIZE);
            local_pool_ray_count = BATCH_SIZE;
        }
        __syncwarp();
        // 32 Rays von globalem Pool holen
        int ray_index = local_pool_next_ray + threadIdx.x;
        if (ray_index >= num_rays)
            return;
        if (threadIdx.x == 0) {
            local_pool_next_ray += 32;
            local_pool_ray_count -= 32;
        }

        // ray holen
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
		float ray_tmin = ray_o4.w;
		float ray_tmax = ray_d4.w;
        float3 ray_id;
        const float ooeps = exp2f(-80.f); // avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        // traverse
        wf::cuda::tri_is closest;
        ifif_traversal_alpha(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, materials, tex_coords, anyhit);
        // Ergebnis speichern
        intersections[ray_index] = closest;
    }
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentwhilewhile_trace(TRACE_PARAMETERS2) {
    global_counter = 0;
    __shared__ volatile int next_ray[MAX_BLOCK_HEIGHT];
    __shared__ volatile int ray_count[MAX_BLOCK_HEIGHT];
    next_ray[threadIdx.y] = 0;
    ray_count[threadIdx.y] = 0;

    volatile int &local_next_ray = next_ray[threadIdx.y];
    volatile int &local_ray_count = ray_count[threadIdx.y];

    while (true) {
        int ray_index;

        // get rays from global to local pool
        if (local_ray_count<=0 && threadIdx.x ==0) {
            local_next_ray = atomicAdd(&global_counter, BATCH_SIZE);
            local_ray_count = BATCH_SIZE;
        }
        __syncwarp();
        // get 32 rays from local pool
        ray_index = local_next_ray + threadIdx.x;
        if (ray_index >= num_rays)
            break;
        if (threadIdx.x == 0) {
            local_next_ray += 32;
            local_ray_count -= 32;
        }

        // fetch ray
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
		float ray_tmin = ray_o4.w;
		float ray_tmax = ray_d4.w;
        float3 ray_id;
        const float ooeps = exp2f(-80.f); // avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        wf::cuda::tri_is closest;
        whilewhile_traversal(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
        intersections[ray_index] = closest;
    }
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentwhilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS2) {
    global_counter = 0;
    __shared__ volatile int next_ray[MAX_BLOCK_HEIGHT];
    __shared__ volatile int ray_count[MAX_BLOCK_HEIGHT];
    next_ray[threadIdx.y] = 0;
    ray_count[threadIdx.y] = 0;

    volatile int &local_next_ray = next_ray[threadIdx.y];
    volatile int &local_ray_count = ray_count[threadIdx.y];

    while (true) {
        int ray_index;

        // get rays from global to local pool
        if (local_ray_count<=0 && threadIdx.x ==0) {
            local_next_ray = atomicAdd(&global_counter, BATCH_SIZE);
            local_ray_count = BATCH_SIZE;
        }
        __syncwarp();
        // get 32 rays from local pool
        ray_index = local_next_ray + threadIdx.x;
        if (ray_index >= num_rays)
            break;
        if (threadIdx.x == 0) {
            local_next_ray += 32;
            local_ray_count -= 32;
        }

        // fetch ray
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
		float ray_tmin = ray_o4.w;
		float ray_tmax = ray_d4.w;
        float3 ray_id;
        const float ooeps = exp2f(-80.f); // avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        wf::cuda::tri_is closest;
        whilewhile_traversal_alpha(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, materials, tex_coords, anyhit);
        intersections[ray_index] = closest;
    }
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void speculativewhilewhile_trace(TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;

    if (i >= resolution.x || j >= resolution.y)
        return;

    // fetch ray
    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
	float ray_tmin = ray_o4.w;
	float ray_tmax = ray_d4.w;
    float3 ray_id;
    const float ooeps = exp2f(-80.f); // avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    speculativewhilewhile_traversal(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void speculativewhilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;

    if (i >= resolution.x || j >= resolution.y)
        return;

    // fetch ray
    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
	float ray_tmin = ray_o4.w;
	float ray_tmax = ray_d4.w;
    float3 ray_id;
    const float ooeps = exp2f(-80.f); // avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    speculativewhilewhile_traversal_alpha(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, materials, tex_coords, anyhit);
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentspeculativewhilewhile_trace(TRACE_PARAMETERS2) {
    global_counter = 0;
    __shared__ volatile int next_ray[MAX_BLOCK_HEIGHT];
    __shared__ volatile int ray_count[MAX_BLOCK_HEIGHT];
    next_ray[threadIdx.y] = 0;
    ray_count[threadIdx.y] = 0;

    volatile int &local_next_ray = next_ray[threadIdx.y];
    volatile int &local_ray_count = ray_count[threadIdx.y];

    while (true) {
        // get rays from global to local pool
        if (local_ray_count<=0 && threadIdx.x ==0) {
            local_next_ray = atomicAdd(&global_counter, BATCH_SIZE);
            local_ray_count = BATCH_SIZE;
        }
        // get 32 rays from local pool
        int ray_index = local_next_ray + threadIdx.x;
        if (ray_index >= num_rays)
            break;
        if (threadIdx.x == 0) {
            local_next_ray += 32;
            local_ray_count -= 32;
        }

        // fetch ray
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
		float ray_tmin = ray_o4.w;
		float ray_tmax = ray_d4.w;
        float3 ray_id;
        const float ooeps = exp2f(-80.f); // avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        // setup traversal
        wf::cuda::tri_is closest;
        speculativewhilewhile_traversal(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
        intersections[ray_index] = closest;
    }
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentspeculativewhilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS2) {
    global_counter = 0;
    __shared__ volatile int next_ray[MAX_BLOCK_HEIGHT];
    __shared__ volatile int ray_count[MAX_BLOCK_HEIGHT];
    next_ray[threadIdx.y] = 0;
    ray_count[threadIdx.y] = 0;

    volatile int &local_next_ray = next_ray[threadIdx.y];
    volatile int &local_ray_count = ray_count[threadIdx.y];

    while (true) {
        // get rays from global to local pool
        if (local_ray_count<=0 && threadIdx.x ==0) {
            local_next_ray = atomicAdd(&global_counter, BATCH_SIZE);
            local_ray_count = BATCH_SIZE;
        }
        // get 32 rays from local pool
        int ray_index = local_next_ray + threadIdx.x;
        if (ray_index >= num_rays)
            break;
        if (threadIdx.x == 0) {
            local_next_ray += 32;
            local_ray_count -= 32;
        }

        // fetch ray
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
		float ray_tmin = ray_o4.w;
		float ray_tmax = ray_d4.w;
        float3 ray_id;
        const float ooeps = exp2f(-80.f); // avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        // setup traversal
        wf::cuda::tri_is closest;
        speculativewhilewhile_traversal_alpha(ray_index, ray_o, ray_d, ray_id, ray_ood, ray_tmin, ray_tmax, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, materials, tex_coords, anyhit);
        intersections[ray_index] = closest;
    }
}

// couldn't reduce dynamicwhilewhile to under 40 Registers, so allow 48 Registers,
// and run one less warp per block to compensate
__launch_bounds__(DESIRED_THREADS_PER_BLOCK - 32 , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void dynamicwhilewhile_trace(TRACE_PARAMETERS2) {
    global_counter = 0;
    // Traversal stack in CUDA thread-local memory
    int stack[STACK_SIZE];
    stack[0] = SENTINEL;

    // Live state during traversal, stored in registers
    int ray_index = -1;
    int stack_pointer;
    int next_node = SENTINEL;
    int postponed_leaf;

    wf::cuda::tri_is closest;
    float3 ray_o, ray_d, ray_id, ray_ood;
	float ray_tmin, ray_tmax;

    // Initialize persistent threads.
    __shared__ volatile int next_ray_array[MAX_BLOCK_HEIGHT]; // Current ray index in global buffer.

    // Persistent threads: fetch and process rays in a loop

    do {    // while (true)
        const int thread_index = threadIdx.x;
        volatile int &ray_base = next_ray_array[threadIdx.y];

        // Fetch new rays from the global pool using lane 0.

        const bool terminated = next_node==SENTINEL;
        const unsigned int mask_terminated = __ballot_sync(0xFFFFFFFF, terminated);
        const int  num_terminated = __popc(mask_terminated);
        const int idx_terminated = __popc(mask_terminated & ((1u<<thread_index)-1));

        if (terminated) {
            int mask = __activemask();
            if (idx_terminated == 0)
                ray_base = atomicAdd(&global_counter, num_terminated);
            __syncwarp(mask);
            ray_index = ray_base + idx_terminated;

            if (ray_index >= num_rays)
                break;  // do-while (true)

            // Fetch ray
            const float4 ray_o4 = FETCH_RAY(rays, ray_index*2 + 0, float4);
            const float4 ray_d4 = FETCH_RAY(rays, ray_index*2 + 1, float4);

            ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
            ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
			ray_tmin = ray_o4.w;
			ray_tmax = ray_d4.w;
            const float ooeps = exp2f(-80.f); // avoid div by zero
            ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d4.x));
            ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d4.y));
            ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d4.z));
            ray_ood = make_float3(ray_o.x * ray_id.x, ray_o.y * ray_id.y, ray_o.z * ray_id.z);

            // Setup traversal
            stack_pointer = 0;
            postponed_leaf = 0;
            next_node = 0;
            //closest.ref = 0;
            closest.set_ref(0);
            closest.t = FLT_MAX;
        }

        // Traversal loop
        while (next_node != SENTINEL) {
            // Traverse internal nodes until all SIMD lanes have found a leaf.

            while (next_node >= 0 && next_node != SENTINEL) {
                // AABBs der Kind-Knoten laden
                const float4 n0xy = FETCH_NODE(bvh_nodes, next_node*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
                const float4 n1xy = FETCH_NODE(bvh_nodes, next_node*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
                const float4 nz =  FETCH_NODE(bvh_nodes, next_node*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
                const float4 tmp = FETCH_NODE(bvh_nodes, next_node*4 + 3, float4);  // child_index0, child_index1
                int2 cnodes = *(int2*)&tmp;

                float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
                float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
                float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
                float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

                // Schnittpunkte mit Kind-Boxen berechnen
                float l_tmin, r_tmin;
                const bool hit_l = intersect_box(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, l_tmin);
                const bool hit_r = intersect_box(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, r_tmin);

                // Neither child was intersected => pop stack.
                if (!hit_l && !hit_r) {
                    next_node = stack[stack_pointer];
                    stack_pointer--;
                }
                // Otherwise => fetch child pointers.
                else {
                    next_node = (hit_l) ? cnodes.x : cnodes.y;

                    // Both children were intersected => push the farther one.
                    if (hit_l && hit_r) {
                        if (r_tmin < l_tmin) {
                            int tmp = next_node;
                            next_node = cnodes.y;
                            cnodes.y = tmp;
                        }
                        stack_pointer++;
                        stack[stack_pointer] = cnodes.y;
                    }
                }

                // First leaf => postpone and continue traversal.
                if (next_node < 0 && postponed_leaf  >= 0) {   // Postpone max 1
                    postponed_leaf = next_node;
                    next_node = stack[stack_pointer];
                    stack_pointer--;
                }

                // All SIMD lanes have found a leaf? => process them.

                if (!__any_sync(__activemask(), postponed_leaf >= 0))
                    break;
            }   // while nodes

            // Process postponed leaf nodes.
            while (postponed_leaf < 0) {
                // TODO how do they do this here?
                const float4 tmp = FETCH_NODE(bvh_nodes, -postponed_leaf*4 + 3, float4);
                const int2 cnodes = *(int2*)&tmp;

                int tri_addr1 = -cnodes.x;
                const int tri_addr2 = tri_addr1 + (-cnodes.y);

                while (tri_addr1 < tri_addr2) { // "while node contains untested primitives"
                    uint  tri_index = FETCH_INDEX(index, tri_addr1, uint);
                    uint4 tri = FETCH_TRI(triangles, tri_index, uint4);

                    float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
                    float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
                    float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

                    float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
                    float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
                    float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

                    wf::cuda::tri_is intersection;
                    if (intersect_triangle(v1, v2, v3,
                                            ray_o, ray_d,
											ray_tmin, ray_tmax,
                                            intersection.t, intersection.beta, intersection.gamma)) {
                        if (intersection.t < closest.t) {
                            closest = intersection;
                            //closest.ref = tri_index;
                            closest.set_ref(tri_index);
                            if (anyhit) next_node = SENTINEL; // terminate ray
                        }
                    }
                    tri_addr1++;
                }

                // Another leaf was postponed => process it as well.
                {
                    postponed_leaf = next_node;
                    if (next_node < 0) {
                        next_node = stack[stack_pointer];
                        stack_pointer--;
                    }
                }

            }   // while leaves

            // DYNAMIC FETCH

            const int num_active  = __popc(__ballot_sync(__activemask(), true));
            if (num_active < DYNAMIC_FETCH_THRESHOLD)
                break;
        }
        intersections[ray_index] = closest;
    } while (true);
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK - 32 , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void dynamicwhilewhile_trace_alpha(ALPHA_TRACE_PARAMETERS2) {
    global_counter = 0;
    // Traversal stack in CUDA thread-local memory
    int stack[STACK_SIZE];
    stack[0] = SENTINEL;

    // Live state during traversal, stored in registers
    int ray_index = -1;
    int stack_pointer;
    int next_node = SENTINEL;
    int postponed_leaf;

    wf::cuda::tri_is closest;
    float3 ray_o, ray_d, ray_id, ray_ood;
	float ray_tmin, ray_tmax;

    // Initialize persistent threads.
    __shared__ volatile int next_ray_array[MAX_BLOCK_HEIGHT]; // Current ray index in global buffer.

    // Persistent threads: fetch and process rays in a loop

    do {    // while (true)
        const int thread_index = threadIdx.x;
        volatile int &ray_base = next_ray_array[threadIdx.y];

        // Fetch new rays from the global pool using lane 0.

        const bool terminated = next_node==SENTINEL;
        const unsigned int mask_terminated = __ballot_sync(0xFFFFFFFF, terminated);
        const int  num_terminated = __popc(mask_terminated);
        const int idx_terminated = __popc(mask_terminated & ((1u<<thread_index)-1));

        if (terminated) {
            int mask = __activemask();
            if (idx_terminated == 0)
                ray_base = atomicAdd(&global_counter, num_terminated);
            __syncwarp(mask);
            ray_index = ray_base + idx_terminated;

            if (ray_index >= num_rays)
                break;  // do-while (true)

            // Fetch ray
            const float4 ray_o4 = FETCH_RAY(rays, ray_index*2 + 0, float4);
            const float4 ray_d4 = FETCH_RAY(rays, ray_index*2 + 1, float4);

            ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
            ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
			ray_tmin = ray_o4.w;
			ray_tmax = ray_d4.w;
            const float ooeps = exp2f(-80.f); // avoid div by zero
            ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d4.x));
            ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d4.y));
            ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d4.z));
            ray_ood = make_float3(ray_o.x * ray_id.x, ray_o.y * ray_id.y, ray_o.z * ray_id.z);

            // Setup traversal
            stack_pointer = 0;
            postponed_leaf = 0;
            next_node = 0;
            //closest.ref = 0;
            closest.set_ref(0);
            closest.t = FLT_MAX;
        }

        // Traversal loop
        while (next_node != SENTINEL) {
            // Traverse internal nodes until all SIMD lanes have found a leaf.

            while (next_node >= 0 && next_node != SENTINEL) {
                // AABBs der Kind-Knoten laden
                const float4 n0xy = FETCH_NODE(bvh_nodes, next_node*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
                const float4 n1xy = FETCH_NODE(bvh_nodes, next_node*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
                const float4 nz =  FETCH_NODE(bvh_nodes, next_node*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
                const float4 tmp = FETCH_NODE(bvh_nodes, next_node*4 + 3, float4);  // child_index0, child_index1
                int2 cnodes = *(int2*)&tmp;

                float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
                float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
                float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
                float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

                // Schnittpunkte mit Kind-Boxen berechnen
                float l_tmin, r_tmin;
                const bool hit_l = intersect_box(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, l_tmin);
                const bool hit_r = intersect_box(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, r_tmin);

                // Neither child was intersected => pop stack.
                if (!hit_l && !hit_r) {
                    next_node = stack[stack_pointer];
                    stack_pointer--;
                }
                // Otherwise => fetch child pointers.
                else {
                    next_node = (hit_l) ? cnodes.x : cnodes.y;

                    // Both children were intersected => push the farther one.
                    if (hit_l && hit_r) {
                        if (r_tmin < l_tmin) {
                            int tmp = next_node;
                            next_node = cnodes.y;
                            cnodes.y = tmp;
                        }
                        stack_pointer++;
                        stack[stack_pointer] = cnodes.y;
                    }
                }

                // First leaf => postpone and continue traversal.
                if (next_node < 0 && postponed_leaf  >= 0) {   // Postpone max 1
                    postponed_leaf = next_node;
                    next_node = stack[stack_pointer];
                    stack_pointer--;
                }

                // All SIMD lanes have found a leaf? => process them.

                if (!__any_sync(__activemask(), postponed_leaf >= 0))
                    break;
            }   // while nodes

            // Process postponed leaf nodes.
            while (postponed_leaf < 0) {
                // TODO how do they do this here?
                const float4 tmp = FETCH_NODE(bvh_nodes, -postponed_leaf*4 + 3, float4);
                const int2 cnodes = *(int2*)&tmp;

                int tri_addr1 = -cnodes.x;
                const int tri_addr2 = tri_addr1 + (-cnodes.y);

                while (tri_addr1 < tri_addr2) { // "while node contains untested primitives"
                    uint  tri_index = FETCH_INDEX(index, tri_addr1, uint);
                    uint4 tri = FETCH_TRI(triangles, tri_index, uint4);

                    float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
                    float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
                    float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

                    float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
                    float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
                    float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

                    wf::cuda::tri_is intersection;
                    if (intersect_triangle(v1, v2, v3,
                                            ray_o, ray_d,
											ray_tmin, ray_tmax,
                                            intersection.t, intersection.beta, intersection.gamma)) {
                        if (!is_below_alpha_threshold(intersection, tri, materials, tex_coords))
                            if (intersection.t < closest.t) {
                                closest = intersection;
                                //closest.ref = tri_index;
                                closest.set_ref(tri_index);
                                if (anyhit) next_node = SENTINEL; // terminate ray
                            }
                    }
                    tri_addr1++;
                }

                // Another leaf was postponed => process it as well.
                {
                    postponed_leaf = next_node;
                    if (next_node < 0) {
                        next_node = stack[stack_pointer];
                        stack_pointer--;
                    }
                }

            }   // while leaves

            // DYNAMIC FETCH

            const int num_active  = __popc(__ballot_sync(__activemask(), true));
            if (num_active < DYNAMIC_FETCH_THRESHOLD)
                break;
        }
        intersections[ray_index] = closest;
    } while (true);
}

// Traversal functions

__forceinline__ __device__ void ifif_traversal(TRAVERSAL_PARAMETERS) {
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    int next_node = 0;
    int tri_addr1 = 0;
    int tri_addr2 = 0;

    stack[0] = SENTINEL;

    while (next_node != SENTINEL || tri_addr1 < tri_addr2) {               // "while ray not terminated"

        // if (node is inner && we are not at bottom of stack)
        if (next_node >= 0 && next_node != SENTINEL) {      // "if node does not contain primitives"
            // AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, next_node*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, next_node*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, next_node*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, next_node*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            // Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersect_box(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersect_box(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r) {
                // TODO is the direct access faster?
                next_node = stack[stack_pointer];
                stack_pointer--;
            }
            else {
                next_node = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if (hit_l && hit_r) {
                    if (r_tmin < l_tmin) {
                        int tmp = next_node;
                        next_node = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }
        }

        // Current node is a leaf: fetch it
        if (next_node < 0 && tri_addr1 >= tri_addr2) {
            const float4 tmp = FETCH_NODE(bvh_nodes, (-next_node)*4+3, float4);
            const int2 cnodes = *(int2*)&tmp;

            tri_addr1  = -cnodes.x;  // stored as int
            tri_addr2  = tri_addr1 + (-cnodes.y);

            // Pop the stack
            next_node = stack[stack_pointer];
            stack_pointer--;
        }

        if (tri_addr1 < tri_addr2) {   // "if node contains untested primitives"
            uint  tri_index = FETCH_INDEX(index, tri_addr1, uint);
            uint4 tri = FETCH_TRI(triangles, tri_index, uint4);

            float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
            float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
            float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

            float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
            float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
            float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

            wf::cuda::tri_is intersection;
            if (intersect_triangle(v1, v2, v3,
                                    ray_o, ray_d,
									ray_tmin, ray_tmax,
                                    intersection.t, intersection.beta, intersection.gamma)) {
                if (intersection.t < hit.t) {
                    hit = intersection;
                    //hit.ref = tri_index;
                    hit.set_ref(tri_index);
                    if (anyhit) return;
                }
            }
            tri_addr1++;
        }
    }
}

__forceinline__ __device__ void ifif_traversal_alpha(ALPHA_TRAVERSAL_PARAMETERS) {
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    int next_node = 0;
    int tri_addr1 = 0;
    int tri_addr2 = 0;

    stack[0] = SENTINEL;

    while (next_node != SENTINEL || tri_addr1 < tri_addr2) {               // "while ray not terminated"

        // if (node is inner && we are not at bottom of stack)
        if (next_node >= 0 && next_node != SENTINEL) {      // "if node does not contain primitives"
            // AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, next_node*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, next_node*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, next_node*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, next_node*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            // Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersect_box(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersect_box(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r) {
                // TODO is the direct access faster?
                next_node = stack[stack_pointer];
                stack_pointer--;
            }
            else {
                next_node = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if (hit_l && hit_r) {
                    if (r_tmin < l_tmin) {
                        int tmp = next_node;
                        next_node = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }
        }

        // Current node is a leaf: fetch it
        if (next_node < 0 && tri_addr1 >= tri_addr2) {
            const float4 tmp = FETCH_NODE(bvh_nodes, (-next_node)*4+3, float4);
            const int2 cnodes = *(int2*)&tmp;

            tri_addr1  = -cnodes.x;  // stored as int
            tri_addr2  = tri_addr1 + (-cnodes.y);

            // Pop the stack
            next_node = stack[stack_pointer];
            stack_pointer--;
        }

        if (tri_addr1 < tri_addr2) {   // "if node contains untested primitives"
            uint  tri_index = FETCH_INDEX(index, tri_addr1, uint);
            uint4 tri = FETCH_TRI(triangles, tri_index, uint4);

            float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
            float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
            float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

            float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
            float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
            float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

            wf::cuda::tri_is intersection;
            if (intersect_triangle(v1, v2, v3,
                                    ray_o, ray_d,
									ray_tmin, ray_tmax,
                                    intersection.t, intersection.beta, intersection.gamma)) {
                
                if (!is_below_alpha_threshold(intersection, tri, materials, tex_coords))
                    if (intersection.t < hit.t) {
                        hit = intersection;
                        //hit.ref = tri_index;
                        hit.set_ref(tri_index);
                        if (anyhit) return;
                    }
            }
            tri_addr1++;
        }
    }
}

__forceinline__ __device__ void whilewhile_traversal(TRAVERSAL_PARAMETERS) {
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    int next_node = 0;

    // setup traversal
    stack[0] = SENTINEL;

    while (next_node != SENTINEL) {   // "while ray not terminated"
        while (next_node >= 0 && next_node != SENTINEL) {  // "while node does not contain primitives"
            // AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, next_node*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, next_node*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, next_node*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, next_node*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            // Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersect_box(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersect_box(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r) {
                // TODO is the direct access faster?
                next_node = stack[stack_pointer];
                stack_pointer--;
            }
            else {
                next_node = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if (hit_l && hit_r) {
                    if (r_tmin < l_tmin) {
                        int tmp = next_node;
                        next_node = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }
        }

        int tri_addr1 = 0;
        int tri_addr2 = 0;
        // Current node is a leaf: fetch it
        if (next_node < 0) {
            const float4 tmp = FETCH_NODE(bvh_nodes, (-next_node)*4+3, float4);
            const int2 cnodes = *(int2*)&tmp;

            tri_addr1  = -cnodes.x;
            tri_addr2 = tri_addr1 + (-cnodes.y);

            // Pop the stack
            next_node = stack[stack_pointer];
            stack_pointer--;
        }


        while (tri_addr1 < tri_addr2) { // "while node contains untested primitives"
            uint  tri_index = FETCH_INDEX(index, tri_addr1, uint);
            uint4 tri = FETCH_TRI(triangles, tri_index, uint4);

            float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
            float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
            float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

            float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
            float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
            float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

            wf::cuda::tri_is intersection;
            if (intersect_triangle(v1, v2, v3,
                                    ray_o, ray_d,
									ray_tmin, ray_tmax,
                                    intersection.t, intersection.beta, intersection.gamma)) {
                if (intersection.t < hit.t) {
                    hit = intersection;
                    //hit.ref = tri_index;
                    hit.set_ref(tri_index);
                    if (anyhit) return;
                }
            }
            tri_addr1++;
        }
    }
}

__forceinline__ __device__ void whilewhile_traversal_alpha(ALPHA_TRAVERSAL_PARAMETERS) {
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    int next_node = 0;

    // setup traversal
    stack[0] = SENTINEL;

    while (next_node != SENTINEL) {   // "while ray not terminated"
        while (next_node >= 0 && next_node != SENTINEL) {  // "while node does not contain primitives"
            // AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, next_node*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, next_node*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, next_node*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, next_node*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            // Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersect_box(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersect_box(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r) {
                // TODO is the direct access faster?
                next_node = stack[stack_pointer];
                stack_pointer--;
            }
            else {
                next_node = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if (hit_l && hit_r) {
                    if (r_tmin < l_tmin) {
                        int tmp = next_node;
                        next_node = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }
        }

        int tri_addr1 = 0;
        int tri_addr2 = 0;
        // Current node is a leaf: fetch it
        if (next_node < 0) {
            const float4 tmp = FETCH_NODE(bvh_nodes, (-next_node)*4+3, float4);
            const int2 cnodes = *(int2*)&tmp;

            tri_addr1  = -cnodes.x;
            tri_addr2 = tri_addr1 + (-cnodes.y);

            // Pop the stack
            next_node = stack[stack_pointer];
            stack_pointer--;
        }


        while (tri_addr1 < tri_addr2) { // "while node contains untested primitives"
            uint  tri_index = FETCH_INDEX(index, tri_addr1, uint);
            uint4 tri = FETCH_TRI(triangles, tri_index, uint4);

            float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
            float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
            float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

            float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
            float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
            float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

            wf::cuda::tri_is intersection;
            if (intersect_triangle(v1, v2, v3,
                                    ray_o, ray_d,
									ray_tmin, ray_tmax,
                                    intersection.t, intersection.beta, intersection.gamma)) {
                if (!is_below_alpha_threshold(intersection, tri, materials, tex_coords))
                    if (intersection.t < hit.t) {
                        hit = intersection;
                        //hit.ref = tri_index;
                        hit.set_ref(tri_index);
                        if (anyhit) return;
                    }
            }
            tri_addr1++;
        }
    }
}

__forceinline__ __device__ void speculativewhilewhile_traversal(TRAVERSAL_PARAMETERS) {
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    stack[0] = SENTINEL;
    int next_node = 0;
    int postponed_leaf = 0;

    while (next_node != SENTINEL) {   // "while ray not terminated"
        bool searching_leaf = true;

        do {  // "while node does not contain primitives"
            // AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, next_node*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, next_node*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, next_node*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, next_node*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            // Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersect_box(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersect_box(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r) {
                // TODO is the direct access faster?
                next_node = stack[stack_pointer];
                stack_pointer--;
            }
            else {
                next_node = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if (hit_l && hit_r) {
                    if (r_tmin < l_tmin) {
                        int tmp = next_node;
                        next_node = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }

            // next_node ist ein Blattknoten (<0) und kein Blatt zwischengespeichert -> Blatt zwischenspeichern und weiter traversieren
            if (next_node < 0 && postponed_leaf == 0) {
                searching_leaf = false;
                postponed_leaf = next_node;
                next_node = stack[stack_pointer];
                --stack_pointer;
            }

            // alle SIMD lanes haben (mindestens) einen Blattknoten gefunden => verarbeiten
            if (!__any_sync(__activemask(), searching_leaf))
                break;
        } while (next_node >= 0 && next_node != SENTINEL); // next_node ist nicht-negativ -> next_node ist ein innerer Knoten

        while (postponed_leaf < 0) {
            // TODO save one load here, how do they do it?
            const float4 tmp = FETCH_NODE(bvh_nodes, -postponed_leaf*4 + 3, float4);  // child_index0, child_index1
            const int2 cnodes = *(int2*)&tmp;

            int tri_addr1 = -cnodes.x;
            const int tri_addr2 = tri_addr1 + (-cnodes.y);

            while (tri_addr1 < tri_addr2) { // "while node contains untested primitives"
                const uint  tri_index = FETCH_INDEX(index, tri_addr1, uint);
                const uint4 tri = FETCH_TRI(triangles, tri_index, uint4);

                const float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
                const float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
                const float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

                const float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
                const float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
                const float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

                wf::cuda::tri_is intersection;
                if (intersect_triangle(v1, v2, v3,
                                        ray_o, ray_d,
										ray_tmin, ray_tmax,
                                        intersection.t, intersection.beta, intersection.gamma)) {
                    if (intersection.t < hit.t) {
                        hit = intersection;
                        //hit.ref = tri_index;
                        hit.set_ref(tri_index);
                        if (anyhit) next_node = SENTINEL; // terminate ray
                    }
                }
                tri_addr1++;
            }

            if (next_node >= 0)    // next_node ist ein innerer Knoten
                // keine Blattknoten mehr zu verarbeiten, loop beenden
                postponed_leaf = 0;
            else {
                postponed_leaf = next_node;
                // pop
                next_node = stack[stack_pointer];
                --stack_pointer;
            }
        }
    }
}

__forceinline__ __device__ void speculativewhilewhile_traversal_alpha(ALPHA_TRAVERSAL_PARAMETERS) {
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    stack[0] = SENTINEL;
    int next_node = 0;
    int postponed_leaf = 0;

    while (next_node != SENTINEL) {   // "while ray not terminated"
        bool searching_leaf = true;

        do {  // "while node does not contain primitives"
            // AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, next_node*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, next_node*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, next_node*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, next_node*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            // Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersect_box(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersect_box(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r) {
                // TODO is the direct access faster?
                next_node = stack[stack_pointer];
                stack_pointer--;
            }
            else {
                next_node = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if (hit_l && hit_r) {
                    if (r_tmin < l_tmin) {
                        int tmp = next_node;
                        next_node = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }

            // next_node ist ein Blattknoten (<0) und kein Blatt zwischengespeichert -> Blatt zwischenspeichern und weiter traversieren
            if (next_node < 0 && postponed_leaf == 0) {
                searching_leaf = false;
                postponed_leaf = next_node;
                next_node = stack[stack_pointer];
                --stack_pointer;
            }

            // alle SIMD lanes haben (mindestens) einen Blattknoten gefunden => verarbeiten
            if (!__any_sync(__activemask(), searching_leaf))
                break;
        } while (next_node >= 0 && next_node != SENTINEL); // next_node ist nicht-negativ -> next_node ist ein innerer Knoten

        while (postponed_leaf < 0) {
            // TODO save one load here, how do they do it?
            const float4 tmp = FETCH_NODE(bvh_nodes, -postponed_leaf*4 + 3, float4);  // child_index0, child_index1
            const int2 cnodes = *(int2*)&tmp;

            int tri_addr1 = -cnodes.x;
            const int tri_addr2 = tri_addr1 + (-cnodes.y);

            while (tri_addr1 < tri_addr2) { // "while node contains untested primitives"
                const uint  tri_index = FETCH_INDEX(index, tri_addr1, uint);
                const uint4 tri = FETCH_TRI(triangles, tri_index, uint4);

                const float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
                const float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
                const float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

                const float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
                const float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
                const float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

                wf::cuda::tri_is intersection;
                if (intersect_triangle(v1, v2, v3,
                                        ray_o, ray_d,
										ray_tmin, ray_tmax,
                                        intersection.t, intersection.beta, intersection.gamma)) {
                    if (!is_below_alpha_threshold(intersection, tri, materials, tex_coords))
                        if (intersection.t < hit.t) {
                            hit = intersection;
                            //hit.ref = tri_index;
                            hit.set_ref(tri_index);
                            if (anyhit) next_node = SENTINEL; // terminate ray
                        }
                }
                tri_addr1++;
            }

            if (next_node >= 0)    // next_node ist ein innerer Knoten
                // keine Blattknoten mehr zu verarbeiten, loop beenden
                postponed_leaf = 0;
            else {
                postponed_leaf = next_node;
                // pop
                next_node = stack[stack_pointer];
                --stack_pointer;
            }
        }
    }
}
