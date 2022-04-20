#include "kernels.h"
#include "cuda-operators.h"


// CLEAR FRAMEBUFFER - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

__global__ void initialize_framebuffer_data(int2 resolution, float4 *framebuffer) {
    int i, j, ray_index;
    i = threadIdx.x + blockIdx.x*blockDim.x;
    j = threadIdx.y + blockIdx.y*blockDim.y;
    ray_index = i + j*resolution.x;

    if (i >= resolution.x || j >= resolution.y)
        return;

	framebuffer[ray_index] = { 0,0,0,0 };
}

void launch_initialize_framebuffer_data(int2 resolution, float4 *framebuffer) {
	initialize_framebuffer_data<<<NUM_BLOCKS_FOR_RESOLUTION(resolution), DESIRED_BLOCK_SIZE>>>(resolution, framebuffer);
}


// SETUP CAMERA RAYS - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

__global__ void setup_rays(glm::vec3 U, glm::vec3 V,
						   float near_w, float near_h,
						   int2 resolution,
						   float3 cam_pos, float3 cam_dir,
						   float2 *random_numbers, float4 *rays) {
    int i, j, ray_index;
    i = threadIdx.x + blockIdx.x*blockDim.x;
    j = threadIdx.y + blockIdx.y*blockDim.y;
    ray_index = i + j*resolution.x;

    if (i >= resolution.x || j >= resolution.y)
        return;


    float3 ray_o = cam_pos;

	// offset \in [0,1]
	float2 offset = random_numbers[ray_index];
    float u = (-1.0f + 2.0f*(float(i)+offset.x)/float(resolution.x)) * near_w;
    float v = (-1.0f + 2.0f*(float(j)+offset.y)/float(resolution.y)) * near_h;

    float3 ray_d { cam_dir.x + u*U.x + v*V.x,
		           cam_dir.y + u*U.y + v*V.y,
				   cam_dir.z + u*U.z + v*V.z };
    normalize(ray_d);

    rays[ray_index*2].x = ray_o.x;
    rays[ray_index*2].y = ray_o.y;
    rays[ray_index*2].z = ray_o.z;

    rays[ray_index*2+1].x = ray_d.x;
    rays[ray_index*2+1].y = ray_d.y;
    rays[ray_index*2+1].z = ray_d.z;
}

void launch_setup_rays(glm::vec3 U, glm::vec3 V,
					   float near_w, float near_h,
					   int2 resolution,
					   float3 cam_pos, float3 cam_dir,
					   float2 *random_numbers, float4 *rays) {
	setup_rays<<<NUM_BLOCKS_FOR_RESOLUTION(resolution), DESIRED_BLOCK_SIZE>>>(U, V, near_w, near_h, resolution, cam_pos, cam_dir,
																			  random_numbers, rays);
}


// SETUP TEST RAYS FOR INCOHERENCY SIMULATION  - - - - - - - - - - - - - - - -

__global__ void setup_ray_incoherent(int2 res,
									 float4 *rays,
									 float3 sphere1, float3 sphere2,
									 float r1, float r2,
									 float r_max,
									 curandStateMtgp32 *rand_state) {
    int max_ray = res.x*res.y;

    for (int ray_index = threadIdx.x + blockIdx.x * blockDim.x; ray_index < max_ray ; ray_index+=blockDim.x*gridDim.x) {
        // et ray origin as center of sphere1
        float3 ray_o = sphere1;
        if (r1 > 0) {
            // pick random point in sphere
            float x = 0, y = 0, z = 0;

            float val1 = curand_uniform(&rand_state[blockIdx.x]);
            float val2 = curand_uniform(&rand_state[blockIdx.x]);
            float val3 = curand_uniform(&rand_state[blockIdx.x]);

            float phi = val1 * 2 * pi; // [0..2pi]
            float costheta = val2 * 2 - 1; // [-1..1]
            float u = val3; // [0..1]

            float theta = acosf(costheta);
            float r = r1*r_max * cbrtf(u);

            x = r * sinf(theta) * cosf(phi);
            y = r * sinf(theta) * sinf(phi);
            z = r * cosf(theta);

            // shift ray origin by random amount
            ray_o.x = ray_o.x + x;
            ray_o.y = ray_o.y + y;
            ray_o.z = ray_o.z + z;
        }

        float3 ray_d;
        if (r2 > 0) {
            // pick random point in sphere
            float x = 0, y = 0, z = 0;

            float val1 = curand_uniform(&rand_state[blockIdx.x]);
            float val2 = curand_uniform(&rand_state[blockIdx.x]);
            float val3 = curand_uniform(&rand_state[blockIdx.x]);

            float phi = val1 * 2 * pi; // [0..2pi]
            float costheta = val2 * 2 - 1; // [-1..1]
            float u = val3; // [0..1]

            float theta = acosf(costheta);
            float r = r2*r_max * cbrtf(u);

            x = r * sinf(theta) * cosf(phi);
            y = r * sinf(theta) * sinf(phi);
            z = r * cosf(theta);

            // shift ray destination (center of sphere2) by random amount and subtract ray origin to get direction vector
            ray_d.x = (sphere2.x + x) - ray_o.x;
            ray_d.y = (sphere2.y + y) - ray_o.y;
            ray_d.z = (sphere2.z + z) - ray_o.z;

        }
        else {
            // point all rays towards center of sphere2
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

void launch_setup_ray_incoherent(int2 config, int2 res,
								 float4 *rays,
								 float3 sphere1, float3 sphere2,
								 float r1, float r2,
								 float r_max,
								 curandStateMtgp32 *rand_state) {
	setup_ray_incoherent<<<config.x, config.y>>>(res, rays, sphere1, sphere2, r1, r2, r_max, rand_state);
}


// ADD INTERSECTION'S SURFACE ALBEDO TO FRAMEBUFFER  - - - - - - - - - - - - -

__global__ void add_hitpoint_albedo(int2 res,
									wf::cuda::tri_is *intersections,
									uint4 *triangles,
									float2 *tex_coords,
									wf::cuda::material *materials,
									float4 *framebuffer) {
	int x = threadIdx.x + blockIdx.x*blockDim.x;
    int y = threadIdx.y + blockIdx.y*blockDim.y;
    int ray_index = y*res.x + x;
    if (x >= res.x || y >= res.y)
        return;

	float4 result { 0,0,0,1 };
	wf::cuda::tri_is hit = intersections[ray_index];
	if (hit.valid()) {
		uint4 tri = triangles[hit.ref];
		if (materials[tri.w].albedo_tex > 0) {
			float2 tc = (1.0f - hit.beta - hit.gamma) * tex_coords[tri.x]
			            + hit.beta * tex_coords[tri.y] 
						+ hit.gamma * tex_coords[tri.z];
			result = tex2D<float4>(materials[tri.w].albedo_tex, tc.x, tc.y);
		}
		else
			result = materials[tri.w].albedo;
		result.w = 1; // be safe
	}
	framebuffer[ray_index] = framebuffer[ray_index] + result;
}

void launch_add_hitpoint_albedo(int2 res,
								wf::cuda::tri_is *intersections,
								uint4 *triangles,
								float2 *tex_coords,
								wf::cuda::material *materials,
								float4 *framebuffer) {
	add_hitpoint_albedo<<<NUM_BLOCKS_FOR_RESOLUTION(res), DESIRED_BLOCK_SIZE>>>(res, intersections, triangles,
																				tex_coords, materials, framebuffer);
}

