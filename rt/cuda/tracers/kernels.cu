#include "kernels.h"
//------------------------------------------------------------------------------
//Global constants
#define STACK_SIZE 64 // Traversal Stack size for all methods
#define BATCH_SIZE 96 // persistent methods: Number of rays to fetch at a time, must be multiple of 32
#define MAX_BLOCK_HEIGHT 10 // Upper bound for blockDim.y. RTX 3090 has 84 SMs -> ceil(sqrt(84))= 10. Will have to be adjusted up for future bigger GPUs
#define DYNAMIC_FETCH_THRESHOLD 20 // If fewer than this active, fetch new rays
#define EPSILON 0.000001 // Moeller-Trumbore triangle intersection
#define SENTINEL 0x76543210 //stack sentinel
//------------------------------------------------------------------------------
//Configures whether Nodes, Rays etc. are loaded through texture pipeline or global memory
// if you change any of these, do a 'make clean', it doesn't work without it for some reason
#define FETCH_NODE(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)
#define FETCH_RAY(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)
#define FETCH_TRI(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)
#define FETCH_INDEX(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)
#define FETCH_VERTEX(NAME, IDX, TYPE) FETCH_GLOBAL(NAME, IDX, TYPE)

//#define FETCH_NODE(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)
// #define FETCH_RAY(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)
// #define FETCH_TRI(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)
//#define FETCH_INDEX(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)
// #define FETCH_VERTEX(NAME, IDX, TYPE) FETCH_TEXTURE(NAME, IDX, TYPE)

#define FETCH_GLOBAL(NAME, IDX, TYPE) ((const TYPE*)NAME)[IDX]
#define FETCH_TEXTURE(NAME, IDX, TYPE) tex1Dfetch<TYPE>(NAME##_tex, IDX)
//------------------------------------------------------------------------------
//Global variables
__device__ int globalCounter; // used in persistent methods
//------------------------------------------------------------------------------
//Ray setup
__global__ void setupRay(glm::vec3 U, glm::vec3 V,
                         float near_w, float near_h,
                         int2 resolution,
                         float3 camPos, float3 camDir,
                         float4* rays){
    int i, j, ray_index;
    i = threadIdx.x + blockIdx.x*blockDim.x;
    j = threadIdx.y + blockIdx.y*blockDim.y;
    ray_index = i + j*resolution.x;

    if(i >= resolution.x || j >= resolution.y ){
        return;
    }

    float2 offset{0,0};
    float3 ray_o;
    float3 ray_d;

    ray_o = camPos;

    float u = (-1.0f + 2.0f*(float(i)+0.5+offset.x)/float(resolution.x)) * near_w;
    float v = (-1.0f + 2.0f*(float(j)+0.5+offset.y)/float(resolution.y)) * near_h;

    ray_d.x = camDir.x + u*U.x + v*V.x;
    ray_d.y = camDir.y + u*U.y + v*V.y;
    ray_d.z = camDir.z + u*U.z + v*V.z;

    normalize(ray_d);

    rays[ray_index*2].x = ray_o.x;
    rays[ray_index*2].y = ray_o.y;
    rays[ray_index*2].z = ray_o.z;

    rays[ray_index*2+1].x = ray_d.x;
    rays[ray_index*2+1].y = ray_d.y;
    rays[ray_index*2+1].z = ray_d.z;
}

__global__ void setupRayIncoherent(int2 res,
                                   float4* rays,
                                   float3 sphere1, float3 sphere2,
                                   float r1, float r2,
                                   float r_max,
                                   curandStateMtgp32* rand_state){
    int max_ray = res.x*res.y;

    for(int ray_index = threadIdx.x + blockIdx.x * blockDim.x; ray_index < max_ray ; ray_index+=blockDim.x*gridDim.x){
        //set ray origin as center of sphere1
        float3 ray_o = sphere1;
        if(r1 > 0){
            //pick random point in sphere
            float x = 0, y = 0, z = 0;

            float val1 = curand_uniform(&rand_state[blockIdx.x]);
            float val2 = curand_uniform(&rand_state[blockIdx.x]);
            float val3 = curand_uniform(&rand_state[blockIdx.x]);

            float phi = val1 * 2 * pi; //[0..2pi]
            float costheta = val2 * 2 - 1; //[-1..1]
            float u = val3; //[0..1]

            float theta = acosf(costheta);
            float r = r1*r_max * cbrtf(u);

            x = r * sinf(theta) * cosf(phi);
            y = r * sinf(theta) * sinf(phi);
            z = r * cosf(theta);

            //shift ray origin by random amount
            ray_o.x = ray_o.x + x;
            ray_o.y = ray_o.y + y;
            ray_o.z = ray_o.z + z;
        }

        float3 ray_d;
        if(r2 > 0){
            //pick random point in sphere
            float x = 0, y = 0, z = 0;

            float val1 = curand_uniform(&rand_state[blockIdx.x]);
            float val2 = curand_uniform(&rand_state[blockIdx.x]);
            float val3 = curand_uniform(&rand_state[blockIdx.x]);

            float phi = val1 * 2 * pi; //[0..2pi]
            float costheta = val2 * 2 - 1; //[-1..1]
            float u = val3; //[0..1]

            float theta = acosf(costheta);
            float r = r2*r_max * cbrtf(u);

            x = r * sinf(theta) * cosf(phi);
            y = r * sinf(theta) * sinf(phi);
            z = r * cosf(theta);

            //shift ray destination (center of sphere2) by random amount and subtract ray origin to get direction vector
            ray_d.x = (sphere2.x + x ) - ray_o.x;
            ray_d.y = (sphere2.y + y ) - ray_o.y;
            ray_d.z = (sphere2.z + z ) - ray_o.z;

        }
        else{
            //point all rays towards center of sphere2
            ray_d.x = sphere2.x - ray_o.x;
            ray_d.y = sphere2.y - ray_o.y;
            ray_d.z = sphere2.z - ray_o.z;
        }

        normalize(ray_d);

        rays[ray_index*2].x = ray_o.x;
        rays[ray_index*2].y = ray_o.y;
        rays[ray_index*2].z = ray_o.z;

        rays[ray_index*2+1].x = ray_d.x;
        rays[ray_index*2+1].y = ray_d.y;
        rays[ray_index*2+1].z = ray_d.z;
    }
}

//Tracers
__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void simpleTrace(int2 resolution, float4* rays, float4* vertex_pos,
                            uint4* triangles, uint32_t* index,
                            wf::cuda::simpleBVHNode* bvh_nodes,
                            wf::cuda::tri_is* intersections,
                            bool anyhit) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if(i >= resolution.x || j >= resolution.y){
        return;
    }

    float4 ray_o4 = FETCH_GLOBAL(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_GLOBAL(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
    float3 ray_id;
    const float ooeps = exp2f(-80.f); //avoid div by zero
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
        wf::cuda::simpleBVHNode node = bvh_nodes[stack[stack_pointer--]];
        if (node.inner()) {

            //Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersectBox(node.box_l_min, node.box_l_max, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, l_tmin);
            const bool hit_r = intersectBox(node.box_r_min, node.box_r_max, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, r_tmin);

            if (hit_l && hit_r){
                if (l_tmin < r_tmin) {
                    stack[++stack_pointer] = node.link_r;
                    stack[++stack_pointer] = node.link_l;
                }
                else {
                    stack[++stack_pointer] = node.link_l;
                    stack[++stack_pointer] = node.link_r;
                }
            }
            else if (hit_l){
                stack[++stack_pointer] = node.link_l;
            }
            else if (hit_r){
                stack[++stack_pointer] = node.link_r;
            }
        }
        else {
            for (int t = 0; t < node.tri_count(); ++t) {
                int tri_index = index[node.tri_offset()+t];
                uint4 tri = triangles[tri_index];

                float3 v1 = make_float3(vertex_pos[tri.x].x, vertex_pos[tri.x].y, vertex_pos[tri.x].z);
                float3 v2 = make_float3(vertex_pos[tri.y].x, vertex_pos[tri.y].y, vertex_pos[tri.y].z);
                float3 v3 = make_float3(vertex_pos[tri.z].x, vertex_pos[tri.z].y, vertex_pos[tri.z].z);

                if (intersectTriangle(v1, v2, v3,
                                        ray_o, ray_d,
                                        0, FLT_MAX,
                                        intersection.t, intersection.beta, intersection.gamma))
                {
                    if(anyhit){ //trace any hit
                        intersections[ray_index] = intersection;
                        return;
                    }
                    else{ //trace closest hit
                        if (intersection.t < closest.t) {
                            closest = intersection;
                            closest.ref = tri_index;
                        }
                    }
                }
            }
        }
    }
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void ififTrace(TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if(i >= resolution.x || j >= resolution.y){
        return;
    }

    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
    float3 ray_id;
    const float ooeps = exp2f(-80.f); //avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    ififTraversal(ray_index, ray_o, ray_d, ray_id, ray_ood, 0, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK, DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void whilewhileTrace(TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;
    if(i >= resolution.x || j >= resolution.y){
        return;
    }

    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
    float3 ray_id;
    const float ooeps = exp2f(-80.f); //avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    whilewhileTraversal(ray_index, ray_o, ray_d, ray_id, ray_ood, 0, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
    intersections[ray_index] = closest;
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentififTrace(TRACE_PARAMETERS2) {
    globalCounter = 0;
    __shared__ volatile int nextRayArray[MAX_BLOCK_HEIGHT];
    __shared__ volatile int rayCountArray[MAX_BLOCK_HEIGHT];
    nextRayArray[threadIdx.y] = 0;
    rayCountArray[threadIdx.y] = 0;

    volatile int& localPoolNextRay = nextRayArray[threadIdx.y];
    volatile int& localPoolRayCount = rayCountArray[threadIdx.y];

    while(true){
        //Rays von globalem in lokalen Pool holen
        if(localPoolRayCount<=0 && threadIdx.x ==0){
            localPoolNextRay = atomicAdd(&globalCounter, BATCH_SIZE);
            localPoolRayCount = BATCH_SIZE;
        }
        __syncwarp();
        //32 Rays von globalem Pool holen
        int ray_index = localPoolNextRay + threadIdx.x;
        if(ray_index >= numRays){
            return;
        }
        if(threadIdx.x == 0){
            localPoolNextRay += 32;
            localPoolRayCount -= 32;
        }

        //ray holen
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
        float3 ray_id;
        const float ooeps = exp2f(-80.f); //avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        //traverse
        wf::cuda::tri_is closest;
        ififTraversal(ray_index, ray_o, ray_d, ray_id, ray_ood, 0, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
        //Ergebnis speichern
        intersections[ray_index] = closest;
    }
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentwhilewhileTrace(TRACE_PARAMETERS2) {
    globalCounter = 0;
    __shared__ volatile int nextRay[MAX_BLOCK_HEIGHT];
    __shared__ volatile int rayCount[MAX_BLOCK_HEIGHT];
    nextRay[threadIdx.y] = 0;
    rayCount[threadIdx.y] = 0;

    volatile int& localNextRay = nextRay[threadIdx.y];
    volatile int& localRayCount = rayCount[threadIdx.y];

    while(true){
        int ray_index;

        //get rays from global to local pool
        if(localRayCount<=0 && threadIdx.x ==0){
            localNextRay = atomicAdd(&globalCounter, BATCH_SIZE);
            localRayCount = BATCH_SIZE;
        }
        __syncwarp();
        //get 32 rays from local pool
        ray_index = localNextRay + threadIdx.x;
        if(ray_index >= numRays){
            break;
        }
        if(threadIdx.x == 0){
            localNextRay += 32;
            localRayCount -= 32;
        }

        //fetch ray
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
        float3 ray_id;
        const float ooeps = exp2f(-80.f); //avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        wf::cuda::tri_is closest;
        whilewhileTraversal(ray_index, ray_o, ray_d, ray_id, ray_ood, 0, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
        intersections[ray_index] = closest;
    }
}

__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void speculativewhilewhileTrace(TRACE_PARAMETERS1) {
    int i = threadIdx.x + blockIdx.x*blockDim.x;
    int j = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = i + j*resolution.x;

    if(i >= resolution.x || j >= resolution.y){
        return;
    }

    //fetch ray
    float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
    float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

    float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
    float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
    float3 ray_id;
    const float ooeps = exp2f(-80.f); //avoid div by zero
    ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
    ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
    ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
    float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

    wf::cuda::tri_is closest;
    speculativewhilewhileTraversal(ray_index, ray_o, ray_d, ray_id, ray_ood, 0, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
    intersections[ray_index] = closest;
}


__launch_bounds__(DESIRED_THREADS_PER_BLOCK , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void persistentspeculativewhilewhileTrace(TRACE_PARAMETERS2) {
    globalCounter = 0;
    __shared__ volatile int nextRay[MAX_BLOCK_HEIGHT];
    __shared__ volatile int rayCount[MAX_BLOCK_HEIGHT];
    nextRay[threadIdx.y] = 0;
    rayCount[threadIdx.y] = 0;

    volatile int& localNextRay = nextRay[threadIdx.y];
    volatile int& localRayCount = rayCount[threadIdx.y];

    while(true){
        //get rays from global to local pool
        if(localRayCount<=0 && threadIdx.x ==0){
            localNextRay = atomicAdd(&globalCounter, BATCH_SIZE);
            localRayCount = BATCH_SIZE;
        }
        //get 32 rays from local pool
        int ray_index = localNextRay + threadIdx.x;
        if(ray_index >= numRays){
            break;
        }
        if(threadIdx.x == 0){
            localNextRay += 32;
            localRayCount -= 32;
        }

        //fetch ray
        float4 ray_o4 = FETCH_RAY(rays, ray_index*2, float4);
        float4 ray_d4 = FETCH_RAY(rays, ray_index*2+1, float4);

        float3 ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
        float3 ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
        float3 ray_id;
        const float ooeps = exp2f(-80.f); //avoid div by zero
        ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d.x));
        ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d.y));
        ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d.z));
        float3 ray_ood = make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);

        //setup traversal
        wf::cuda::tri_is closest;
        speculativewhilewhileTraversal(ray_index, ray_o, ray_d, ray_id, ray_ood, 0, bvh_nodes, bvh_nodes_tex, index, index_tex, triangles, triangles_tex, vertex_pos, vertex_pos_tex, closest, anyhit);
        intersections[ray_index] = closest;
    }
}

// couldn't reduce dynamicwhilewhile to under 40 Registers, so allow 48 Registers,
// and run one less warp per block to compensate
__launch_bounds__(DESIRED_THREADS_PER_BLOCK - 32 , DESIRED_BLOCKS_PER_MULTIPROCESSOR)
__global__ void dynamicwhilewhileTrace(TRACE_PARAMETERS2) {
    globalCounter = 0;
    //Traversal stack in CUDA thread-local memory
    int stack[STACK_SIZE];
    stack[0] = SENTINEL;

    //Live state during traversal, stored in registers
    int ray_index = -1;
    int stack_pointer;
    int nextNode = SENTINEL;
    int postponedLeaf;

    wf::cuda::tri_is closest;
    float3 ray_o, ray_d, ray_id, ray_ood;

    //Initialize persistent threads.
    __shared__ volatile int nextRayArray[MAX_BLOCK_HEIGHT]; // Current ray index in global buffer.

    //Persistent threads: fetch and process rays in a loop

    do {    //while (true)
        const int thread_index = threadIdx.x;
        volatile int& rayBase = nextRayArray[threadIdx.y];

        //Fetch new rays from the global pool using lane 0.

        const bool terminated = nextNode==SENTINEL;
        const unsigned int maskTerminated = __ballot_sync(0xFFFFFFFF, terminated);
        const int  numTerminated = __popc(maskTerminated);
        const int idxTerminated = __popc(maskTerminated & ((1u<<thread_index)-1));

        if(terminated){
            int mask = __activemask();
            if(idxTerminated == 0){
                rayBase = atomicAdd(&globalCounter, numTerminated);
            }
            __syncwarp(mask);
            ray_index = rayBase + idxTerminated;

            if(ray_index >= numRays){
                break;  //do-while(true)
            }

            //Fetch ray
            const float4 ray_o4 = FETCH_RAY(rays, ray_index*2 + 0, float4);
            const float4 ray_d4 = FETCH_RAY(rays, ray_index*2 + 1, float4);

            ray_o = make_float3(ray_o4.x, ray_o4.y, ray_o4.z);
            ray_d = make_float3(ray_d4.x, ray_d4.y, ray_d4.z);
            const float ooeps = exp2f(-80.f); //avoid div by zero
            ray_id.x = 1.0f/ (fabsf(ray_d4.x) > ooeps ? ray_d4.x : copysignf(ooeps, ray_d4.x));
            ray_id.y = 1.0f/ (fabsf(ray_d4.y) > ooeps ? ray_d4.y : copysignf(ooeps, ray_d4.y));
            ray_id.z = 1.0f/ (fabsf(ray_d4.z) > ooeps ? ray_d4.z : copysignf(ooeps, ray_d4.z));
            ray_ood = make_float3(ray_o.x * ray_id.x, ray_o.y * ray_id.y, ray_o.z * ray_id.z);

            // Setup traversal
            stack_pointer = 0;
            postponedLeaf = 0;
            nextNode = 0;
            closest.ref = 0;
            closest.t = FLT_MAX;
        }

        //Traversal loop
        while(nextNode != SENTINEL){
            // Traverse internal nodes until all SIMD lanes have found a leaf.

            while(nextNode >= 0 && nextNode != SENTINEL){
                //AABBs der Kind-Knoten laden
                const float4 n0xy = FETCH_NODE(bvh_nodes, nextNode*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
                const float4 n1xy = FETCH_NODE(bvh_nodes, nextNode*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
                const float4 nz =  FETCH_NODE(bvh_nodes, nextNode*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
                const float4 tmp = FETCH_NODE(bvh_nodes, nextNode*4 + 3, float4);  // child_index0, child_index1
                int2 cnodes = *(int2*)&tmp;

                float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
                float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
                float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
                float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

                //Schnittpunkte mit Kind-Boxen berechnen
                float l_tmin, r_tmin;
                const bool hit_l = intersectBox(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, l_tmin);
                const bool hit_r = intersectBox(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, closest.t, r_tmin);

                // Neither child was intersected => pop stack.
                if (!hit_l && !hit_r)
                {
                    nextNode = stack[stack_pointer];
                    stack_pointer--;
                }

                // Otherwise => fetch child pointers.
                else
                {
                    nextNode = (hit_l) ? cnodes.x : cnodes.y;

                    // Both children were intersected => push the farther one.

                    if (hit_l && hit_r)
                    {
                        if (r_tmin < l_tmin){
                            int tmp = nextNode;
                            nextNode = cnodes.y;
                            cnodes.y = tmp;
                        }
                        stack_pointer++;
                        stack[stack_pointer] = cnodes.y;
                    }
                }

                // First leaf => postpone and continue traversal.
                if (nextNode < 0 && postponedLeaf  >= 0)     // Postpone max 1
                {
                    postponedLeaf = nextNode;
                    nextNode = stack[stack_pointer];
                    stack_pointer--;
                }

                // All SIMD lanes have found a leaf? => process them.

                if(!__any_sync(__activemask(), postponedLeaf >= 0)){
                    break;
                }
            }   // while nodes

            // Process postponed leaf nodes.
            while (postponedLeaf < 0)
            {
                //TODO how do they do this here?
                const float4 tmp = FETCH_NODE(bvh_nodes, -postponedLeaf*4 + 3, float4);
                const int2 cnodes = *(int2*)&tmp;

                int triAddr1 = -cnodes.x;
                const int triAddr2 = triAddr1 + (-cnodes.y);

                while(triAddr1 < triAddr2) { // "while node contains untested primitives"
                    uint1 tri_index = FETCH_INDEX(index, triAddr1 , uint1);
                    uint4 tri = FETCH_TRI(triangles, tri_index.x, uint4);

                    float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
                    float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
                    float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

                    float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
                    float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
                    float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

                    wf::cuda::tri_is intersection;
                    if (intersectTriangle(v1, v2, v3,
                                            ray_o, ray_d,
                                            0, FLT_MAX,
                                            intersection.t, intersection.beta, intersection.gamma))
                    {
                        if (intersection.t < closest.t) {
                            closest = intersection;
                            closest.ref = tri_index.x;
                            if(anyhit) nextNode = SENTINEL; //terminate ray
                        }
                    }
                    triAddr1++;
                }

                // Another leaf was postponed => process it as well.

                {
                    postponedLeaf = nextNode;
                    if (nextNode < 0)
                    {
                        nextNode = stack[stack_pointer];
                        stack_pointer--;
                    }
                }

            }   // while leaves

            // DYNAMIC FETCH

            const int numActive  = __popc(__ballot_sync(__activemask(), true));
            if(numActive < DYNAMIC_FETCH_THRESHOLD){
                break;
            }
        }
        intersections[ray_index] = closest;
    } while(true);
}


//Traversal functions

__forceinline__ __device__ void ififTraversal(TRAVERSAL_PARAMETERS){
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    int nextNode = 0;
    int triAddr1 = 0;
    int triAddr2 = 0;

    stack[0] = SENTINEL;

    while (nextNode != SENTINEL || triAddr1 < triAddr2) {               //"while ray not terminated"

        // if(node is inner && we are not at bottom of stack)
        if (nextNode >= 0 && nextNode != SENTINEL) {      //"if node does not contain primitives"
            //AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, nextNode*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, nextNode*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, nextNode*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, nextNode*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            //Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersectBox(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersectBox(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r){
                //TODO is the direct access faster?
                nextNode = stack[stack_pointer];
                stack_pointer--;
            }
            else{
                nextNode = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if( hit_l && hit_r ){
                    if (r_tmin < l_tmin){
                        int tmp = nextNode;
                        nextNode = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }
        }

        // Current node is a leaf: fetch it
        if (nextNode < 0 && triAddr1 >= triAddr2)
        {
            const float4 tmp = FETCH_NODE(bvh_nodes, (-nextNode)*4+3, float4);
            const int2 cnodes = *(int2*)&tmp;

            triAddr1  = -cnodes.x;  // stored as int
            triAddr2  = triAddr1 + (-cnodes.y);

            // Pop the stack
            nextNode = stack[stack_pointer];
            stack_pointer--;
        }

        if(triAddr1 < triAddr2){   // "if node contains untested primitives"
            uint1 tri_index = FETCH_INDEX(index, triAddr1 , uint1);
            uint4 tri = FETCH_TRI(triangles, tri_index.x, uint4);

            float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
            float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
            float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

            float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
            float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
            float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

            wf::cuda::tri_is intersection;
            if (intersectTriangle(v1, v2, v3,
                                    ray_o, ray_d,
                                    0, FLT_MAX,
                                    intersection.t, intersection.beta, intersection.gamma))
            {
                if (intersection.t < hit.t) {
                    hit = intersection;
                    hit.ref = tri_index.x;
                    if(anyhit) return;
                }
            }
            triAddr1++;
        }
    }
}

__forceinline__ __device__ void whilewhileTraversal(TRAVERSAL_PARAMETERS){
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    int nextNode = 0;

    //setup traversal
    stack[0] = SENTINEL;

    while (nextNode != SENTINEL) {   // "while ray not terminated"
        while (nextNode >= 0 && nextNode != SENTINEL) {  // "while node does not contain primitives"
            //AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, nextNode*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, nextNode*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, nextNode*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, nextNode*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            //Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersectBox(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersectBox(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r){
                //TODO is the direct access faster?
                nextNode = stack[stack_pointer];
                stack_pointer--;
            }
            else{
                nextNode = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if( hit_l && hit_r ){
                    if (r_tmin < l_tmin){
                        int tmp = nextNode;
                        nextNode = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }
        }

        int triAddr1 = 0;
        int triAddr2 = 0;
        // Current node is a leaf: fetch it
        if (nextNode < 0)
        {
            const float4 tmp = FETCH_NODE(bvh_nodes, (-nextNode)*4+3, float4);
            const int2 cnodes = *(int2*)&tmp;

            triAddr1  = -cnodes.x;
            triAddr2 = triAddr1 + (-cnodes.y);

            // Pop the stack
            nextNode = stack[stack_pointer];
            stack_pointer--;
        }


        while( triAddr1 < triAddr2) { // "while node contains untested primitives"
            uint1 tri_index = FETCH_INDEX(index, triAddr1 , uint1);
            uint4 tri = FETCH_TRI(triangles, tri_index.x, uint4);

            float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
            float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
            float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

            float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
            float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
            float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

            wf::cuda::tri_is intersection;
            if (intersectTriangle(v1, v2, v3,
                                    ray_o, ray_d,
                                    0, FLT_MAX,
                                    intersection.t, intersection.beta, intersection.gamma))
            {
                if (intersection.t < hit.t) {
                    hit = intersection;
                    hit.ref = tri_index.x;
                    if(anyhit) return;
                }
            }
            triAddr1++;
        }
    }
}

__forceinline__ __device__ void speculativewhilewhileTraversal(TRAVERSAL_PARAMETERS){
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    stack[0] = SENTINEL;
    int nextNode = 0;
    int postponedLeaf = 0;

    while (nextNode != SENTINEL) {   // "while ray not terminated"
        bool searchingLeaf = true;

        do {  // "while node does not contain primitives"
            //AABBs der Kind-Knoten laden
            const float4 n0xy = FETCH_NODE(bvh_nodes, nextNode*4 + 0, float4); // (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
            const float4 n1xy = FETCH_NODE(bvh_nodes, nextNode*4 + 1, float4); // (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
            const float4 nz =  FETCH_NODE(bvh_nodes, nextNode*4 + 2, float4);  // (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
            const float4 tmp = FETCH_NODE(bvh_nodes, nextNode*4 + 3, float4);  // child_index0, child_index1
            int2 cnodes = *(int2*)&tmp;

            float3 l_boxmin = make_float3(n0xy.x, n0xy.z, nz.x);
            float3 l_boxmax = make_float3(n0xy.y, n0xy.w, nz.y);
            float3 r_boxmin = make_float3(n1xy.x, n1xy.z, nz.z);
            float3 r_boxmax = make_float3(n1xy.y, n1xy.w, nz.w);

            //Schnittpunkte mit Kind-Boxen berechnen
            float l_tmin, r_tmin;
            const bool hit_l = intersectBox(l_boxmin, l_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, l_tmin);
            const bool hit_r = intersectBox(r_boxmin, r_boxmax, ray_o, ray_d, ray_id, ray_ood, 0, hit.t, r_tmin);

            // keiner der Kind-Knoten wurde getroffen, nächsten Knoten vom Stack holen
            if (!hit_l && !hit_r){
                //TODO is the direct access faster?
                nextNode = stack[stack_pointer];
                stack_pointer--;
            }
            else{
                nextNode = (hit_l) ? cnodes.x : cnodes.y;

                // beide Kind-Knoten wurden getroffen, das weiter entfernte auf den Stack legen
                if( hit_l && hit_r ){
                    if (r_tmin < l_tmin){
                        int tmp = nextNode;
                        nextNode = cnodes.y;
                        cnodes.y = tmp;
                    }
                    stack_pointer++;
                    stack[stack_pointer] = cnodes.y;
                }
            }

            // nextNode ist ein Blattknoten (<0) und kein Blatt zwischengespeichert -> Blatt zwischenspeichern und weiter traversieren
            if(nextNode < 0 && postponedLeaf == 0){
                searchingLeaf = false;
                postponedLeaf = nextNode;
                nextNode = stack[stack_pointer];
                --stack_pointer;
            }

            // alle SIMD lanes haben (mindestens) einen Blattknoten gefunden => verarbeiten
            if(!__any_sync(__activemask(), searchingLeaf)){
                break;
            }
        } while( nextNode >= 0 && nextNode != SENTINEL); // nextNode ist nicht-negativ -> nextNode ist ein innerer Knoten

        while(postponedLeaf < 0){
            //TODO save one load here, how do they do it?
            const float4 tmp = FETCH_NODE(bvh_nodes, -postponedLeaf*4 + 3, float4);  // child_index0, child_index1
            const int2 cnodes = *(int2*)&tmp;

            int triAddr1 = -cnodes.x;
            const int triAddr2 = triAddr1 + (-cnodes.y);

            while(triAddr1 < triAddr2) { // "while node contains untested primitives"
                const uint1 tri_index = FETCH_INDEX(index, triAddr1 , uint1);
                const uint4 tri = FETCH_TRI(triangles, tri_index.x, uint4);

                const float4 _v1 = FETCH_VERTEX(vertex_pos, tri.x, float4);
                const float4 _v2 = FETCH_VERTEX(vertex_pos, tri.y, float4);
                const float4 _v3 = FETCH_VERTEX(vertex_pos, tri.z, float4);

                const float3 v1 = make_float3(_v1.x, _v1.y, _v1.z);
                const float3 v2 = make_float3(_v2.x, _v2.y, _v2.z);
                const float3 v3 = make_float3(_v3.x, _v3.y, _v3.z);

                wf::cuda::tri_is intersection;
                if (intersectTriangle(v1, v2, v3,
                                        ray_o, ray_d,
                                        0, FLT_MAX,
                                        intersection.t, intersection.beta, intersection.gamma))
                {
                    if (intersection.t < hit.t) {
                        hit = intersection;
                        hit.ref = tri_index.x;
                        if(anyhit) nextNode = SENTINEL; //terminate ray
                    }
                }
                triAddr1++;
            }

            if(nextNode >= 0){   //nextNode ist ein innerer Knoten
                //keine Blattknoten mehr zu verarbeiten, loop beenden
                postponedLeaf = 0;
            }
            else{
                postponedLeaf = nextNode;
                //pop
                nextNode = stack[stack_pointer];
                --stack_pointer;
            }
        }
    }
}

//Intersection tests
__forceinline__ __device__ bool intersectBox(INTERSECT_BOX_PARAMETERS){
    //return intersectBoxShirley(boxmin, boxmax, ray_o, ray_d, ray_id, ray_ood, t_min, t_max, hitT);
    return intersectBoxAila(boxmin, boxmax, ray_o, ray_d, ray_id, ray_ood, t_min, t_max, hitT);
}
__forceinline__ __device__ bool intersectBoxShirley(INTERSECT_BOX_PARAMETERS){
    const float t1x_tmp = (boxmin.x - ray_o.x) * ray_id.x;
    const float t2x_tmp = (boxmax.x - ray_o.x) * ray_id.x;
    const float t1x = (t1x_tmp < t2x_tmp) ? t1x_tmp : t2x_tmp;
    const float t2x = (t2x_tmp < t1x_tmp) ? t1x_tmp : t2x_tmp;

    const float t1y_tmp = (boxmin.y - ray_o.y) * ray_id.y;
    const float t2y_tmp = (boxmax.y - ray_o.y) * ray_id.y;
    const float t1y = (t1y_tmp < t2y_tmp) ? t1y_tmp : t2y_tmp;
    const float t2y = (t2y_tmp < t1y_tmp) ? t1y_tmp : t2y_tmp;

    const float t1z_tmp = (boxmin.z - ray_o.z) * ray_id.z;
    const float t2z_tmp = (boxmax.z - ray_o.z) * ray_id.z;
    const float t1z = (t1z_tmp < t2z_tmp) ? t1z_tmp : t2z_tmp;
    const float t2z = (t2z_tmp < t1z_tmp) ? t1z_tmp : t2z_tmp;

    float t1 = (t1x < t1y) ? t1y : t1x;
        t1 = (t1z < t1 ) ? t1  : t1z;
    float t2 = (t2x < t2y) ? t2x : t2y;
        t2 = (t2z < t2 ) ? t2z : t2;

    if (t1 > t2)    return false;
    if (t2 < t_min) return false;
    if (t1 > t_max) return false;

    hitT = t1;
    return true;
}
__forceinline__ __device__ bool intersectBoxAila(INTERSECT_BOX_PARAMETERS){
    // Following Aila, Laine, Karras: Understanding the efficiency of ray traversal on GPUs–Kepler and Fermi addendum

    float x0 = boxmin.x * ray_id.x - ray_ood.x;
    float y0 = boxmin.y * ray_id.y - ray_ood.y;
    float z0 = boxmin.z * ray_id.z - ray_ood.z;

    float x1 = boxmax.x * ray_id.x - ray_ood.x;
    float y1 = boxmax.y * ray_id.y - ray_ood.y;
    float z1 = boxmax.z * ray_id.z - ray_ood.z;

    //Using VMIN/VMAX assembly instructions
    //A) -Kepler code variant (Aila/Laine) (this requires t_min to be nonnegative to be correct) - 72ms
    // float tminbox = __int_as_float(
    //                     vmax_max(
    //                     __float_as_int(fminf(x0,x1)),
    //                     __float_as_int(fminf(y0,y1)),
    //                     vmin_max(
    //                         __float_as_int(z0),
    //                         __float_as_int(z1),
    //                         __float_as_int(t_min))));
    // float tmaxbox = __int_as_float(
    //                     vmin_min(
    //                         __float_as_int(fmaxf(x0,x1)),
    //                         __float_as_int(fmaxf(y0,y1)),
    //                         vmax_min(
    //                             __float_as_int(z0),
    //                             __float_as_int(z1),
    //                             __float_as_int(t_max))));

    //B) -Fermi code variant (Aila/Laine) (this requires t_min to be nonnegative to be correct) - 87ms
    // float tminbox = __int_as_float(
    //                     vmin_max(
    //                         __float_as_int(z0),
    //                         __float_as_int(z1),
    //                         vmin_max(
    //                             __float_as_int(y0),
    //                             __float_as_int(y1),
    //                             vmin_max(
    //                                 __float_as_int(x0),
    //                                 __float_as_int(x1),
    //                                 __float_as_int(t_min)))));
    // float tmaxbox = __int_as_float(
    //                     vmax_min(
    //                         __float_as_int(z0),
    //                         __float_as_int(z1),
    //                         vmax_min(
    //                             __float_as_int(y0),
    //                             __float_as_int(y1),
    //                             vmax_min(
    //                                 __float_as_int(x0),
    //                                 __float_as_int(x1),
    //                                 __float_as_int(t_max)))));

    // C) -Paper variant VMIN/VMAX (this requires t_min to be nonnegative to be correct) - 87ms
    // float tminbox = __int_as_float(
    //                     vmin_max(
    //                         __float_as_int(x0),
    //                         __float_as_int(x1),
    //                         vmin_max(
    //                             __float_as_int(y0),
    //                             __float_as_int(y1),
    //                             vmin_max(
    //                                 __float_as_int(z0),
    //                                 __float_as_int(z1),
    //                                 __float_as_int(t_min)))));
    // float tmaxbox = __int_as_float(
    //                     vmax_min(
    //                     __float_as_int(x0),
    //                     __float_as_int(x1),
    //                     vmax_min(
    //                         __float_as_int(y0),
    //                         __float_as_int(y1),
    //                         vmax_min(
    //                             __float_as_int(z0),
    //                             __float_as_int(z1),
    //                             __float_as_int(t_max)))));

    //D) Using cuda integer instructions (this requires t_min to be nonnegative to be correct) - 38ms
    // float tminbox = __int_as_float(max(
    //                                     max(
    //                                         __float_as_int(t_min),
    //                                         min(__float_as_int(x0),
    //                                             __float_as_int(x1))),
    //                                     max(
    //                                         min(__float_as_int(y0),
    //                                             __float_as_int(y1)),
    //                                         min(__float_as_int(z0),
    //                                             __float_as_int(z1)))));
    // float tmaxbox = __int_as_float(min(
    //                                     min(
    //                                         __float_as_int(t_max),
    //                                         max(
    //                                             __float_as_int(x0),
    //                                             __float_as_int(x1))),
    //                                     min(
    //                                         max(
    //                                             __float_as_int(y0),
    //                                             __float_as_int(y1)),
    //                                         max(
    //                                             __float_as_int(z0),
    //                                             __float_as_int(z1)))));

    //E) -Tesla code variant (Aila/Laine), but only standard instructions - 38ms
    // float tminbox = fmaxf(
    //                     fmaxf(
    //                         fmaxf(
    //                             fminf(x0,x1),
    //                             fminf(y0,y1)),
    //                         fminf(z0,z1)),
    //                     t_min);
    // float tmaxbox = fminf(
    //                     fminf(
    //                         fminf(
    //                             fmaxf(x0,x1),
    //                             fmaxf(y0,y1)),
    //                         fmaxf(z0,z1)),
    //                     t_max);

    //F) Using standard cuda float math instructions - 38ms
    float tminbox = fmaxf(
                            fmaxf(
                                    t_min,
                                    fminf(x0,x1)
                                 ),
                            fmaxf(
                                    fminf(y0,y1),
                                    fminf(z0,z1)
                                )
                        );
    float tmaxbox = fminf(
                            fminf(
                                    t_max,
                                    fmaxf(x0,x1)
                                ),
                            fminf(
                                    fmaxf(y0,y1),
                                    fmaxf(z0,z1)
                                )
                        );

    bool intersect = (tmaxbox >= tminbox);
    hitT = tminbox;
    return intersect;
}

__forceinline__ __device__ bool intersectTriangle(INTERSECT_TRIANGLE_PARAMETERS){
    //return intersectTriangleShirley(v1, v2, v3, ray_o, ray_d, t_min, t_max, hitT, hitBeta, hitGamma);
    return intersectTriangleMoellerTrumbore(v1, v2, v3, ray_o, ray_d, t_min, t_max, hitT, hitBeta, hitGamma);
}

__forceinline__ __device__ bool intersectTriangleShirley(INTERSECT_TRIANGLE_PARAMETERS){
    // Following Shirley: Fundamentals of Computer Graphics 4th ed., pp. 77-79
    const float a_x = v1.x;
    const float a_y = v1.y;
    const float a_z = v1.z;

    const float a = a_x - v2.x;
    const float b = a_y - v2.y;
    const float c = a_z - v2.z;

    const float d = a_x - v3.x;
    const float e = a_y - v3.y;
    const float f = a_z - v3.z;

    const float g = ray_d.x;
    const float h = ray_d.y;
    const float i = ray_d.z;

    const float j = a_x - ray_o.x;
    const float k = a_y - ray_o.y;
    const float l = a_z - ray_o.z;

    float common1 = e*i - h*f;
    float common2 = g*f - d*i;
    float common3 = d*h - e*g;
    float M 	  = a * common1  +  b * common2  +  c * common3;
    float beta 	  = j * common1  +  k * common2  +  l * common3;

    common1       = a*k - j*b;
    common2       = j*c - a*l;
    common3       = b*l - k*c;
    float gamma   = i * common1  +  h * common2  +  g * common3;
    float tt    = -(f * common1  +  e * common2  +  d * common3);

    beta /= M;
    gamma /= M;
    tt /= M;

    if (tt > t_min && tt < t_max)
        if (beta > 0 && gamma > 0 && beta + gamma <= 1)
        {
            hitT = tt;
            hitBeta = beta;
            hitGamma = gamma;
            return true;
        }
    return false;
}

__forceinline__ __device__ bool intersectTriangleMoellerTrumbore(INTERSECT_TRIANGLE_PARAMETERS){
    // following Möller, Trumbore: Fast, Minimum Storage Ray/Triangle Intersection
    float3 edge1, edge2, tvec, pvec, qvec;
    float det, inv_det;

    edge1.x = v2.x - v1.x;
    edge1.y = v2.y - v1.y;
    edge1.z = v2.z - v1.z;

    edge2.x = v3.x - v1.x;
    edge2.y = v3.y - v1.y;
    edge2.z = v3.z - v1.z;

    cross(pvec, ray_d, edge2);
    det = dot(edge1, pvec);

    if(det > -EPSILON && det < EPSILON){
        return false;
    }

    tvec.x = ray_o.x - v1.x;
    tvec.y = ray_o.y - v1.y;
    tvec.z = ray_o.z - v1.z;

    hitBeta = dot(tvec, pvec);

    if( hitBeta < 0.0 || hitBeta > det ){
        return false;
    }

    cross(qvec, tvec, edge1);
    hitGamma = dot(ray_d, qvec);
    if( hitGamma < 0.0 || hitBeta + hitGamma > det){
        return false;
    }

    hitT = dot(edge2, qvec);
    inv_det = 1.0/det;
    hitT *= inv_det;
    hitBeta *= inv_det;
    hitGamma *= inv_det;
    return true;
}

//Math Helpers
__forceinline__ __device__ void normalize(float3 &v) {
    float invLen = rsqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    v.x = v.x * invLen;
    v.y = v.y * invLen;
    v.z = v.z * invLen;
}

__forceinline__ __device__ __host__ float dot(const float3 &a, const float3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__forceinline__ __device__ __host__ void cross(float3 &dest, const float3 &a, const float3 &b) {
    dest.x = a.y*b.z - a.z*b.y;
    dest.y = a.z*b.x - a.x*b.z;
    dest.z =  a.x*b.y - a.y*b.x;
}

__forceinline__  __device__ __host__ float3 cross(const float3 &a, const float3 &b) {
    return make_float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

 __forceinline__ __device__ int vmin_max (int a, int b, int c) {
    int ret;
    asm("vmin.s32.s32.s32.max %0, %1, %2, %3;" : "=r"(ret) : "r"(a), "r"(b), "r"(c));
    return ret;
}
__forceinline__ __device__ int vmax_min (int a, int b, int c) {
    int val;
    asm("vmax.s32.s32.s32.min %0, %1, %2, %3;" : "=r"(val) : "r"(a), "r"(b), "r"(c));
    return val;
}
__forceinline__ __device__ int vmin_min (int a, int b, int c) {
    int val;
    asm("vmin.s32.s32.s32.min %0, %1, %2, %3;" : "=r"(val) : "r"(a), "r"(b), "r"(c));
    return val;
}
__forceinline__ __device__ int vmax_max (int a, int b, int c) {
    int val;
    asm("vmax.s32.s32.s32.max %0, %1, %2, %3;" : "=r"(val) : "r"(a), "r"(b), "r"(c));
    return val;
}