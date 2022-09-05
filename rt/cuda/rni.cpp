#include "rni.h"

#include "platform.h"

#include "libgi/timer.h"

#include <iostream>

#include "kernels.h"
#include "cuda-helpers.h"

#define CURAND_CALL(x) do { if((x)!=CURAND_STATUS_SUCCESS) { printf("Error at %s:%d\n",__FILE__,__LINE__); exit(99); }} while(0)

namespace wf {
	namespace cuda {
		// batch_cam_ray_setup::batch_cam_ray_setup() {}

		batch_cam_ray_setup::batch_cam_ray_setup() {
		}
		
		batch_cam_ray_setup::~batch_cam_ray_setup() {
		}

		void batch_cam_ray_setup::run() {
			time_this_wf_step;

			int2 resolution{rc->resolution().x, rc->resolution().y};

			if (pf->rt->use_incoherence) {
				// find max/min scene extent (scene size)
				float3 scene_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
				float3 scene_min = {FLT_MAX, FLT_MAX, FLT_MAX};
				for (auto vertex : rc->scene.vertices) {
					if (vertex.pos.x < scene_min.x) scene_min.x = vertex.pos.x;
					if (vertex.pos.y < scene_min.y) scene_min.y = vertex.pos.y;
					if (vertex.pos.z < scene_min.z) scene_min.z = vertex.pos.z;
					if (vertex.pos.x > scene_max.x) scene_max.x = vertex.pos.x;
					if (vertex.pos.y > scene_max.y) scene_max.y = vertex.pos.y;
					if (vertex.pos.z > scene_max.z) scene_max.z = vertex.pos.z;
				}
				float3 scene_size = { abs(scene_max.x - scene_min.x), abs(scene_max.y - scene_min.y), abs(scene_max.z - scene_min.z)};

				// find biggest scene dimension and set sphere centers
				float r_max;
				float3 sphere1, sphere2;
				if (scene_size.x >= scene_size.y && scene_size.x >= scene_size.z) {
					r_max = scene_size.x/4;
					// in the two smaller dimensions, center the sphere. in the largest dimension position one sphere in one quarter point, the other sphere in the other
					sphere1 = { scene_min.x + scene_size.x/4, scene_min.y + scene_size.y/2 , scene_min.z + scene_size.z/2 };
					sphere2 = { scene_max.x - scene_size.x/4, scene_min.y + scene_size.y/2 , scene_min.z + scene_size.z/2 };
				}
				else if (scene_size.y >= scene_size.x && scene_size.y >= scene_size.z) {
					r_max = scene_size.y/4;
					sphere1 = { scene_min.x + scene_size.x/2, scene_min.y + scene_size.y/4 , scene_min.z + scene_size.z/2 };
					sphere2 = { scene_min.x + scene_size.x/2, scene_max.y - scene_size.y/4 , scene_min.z + scene_size.z/2 };
				}
				else if (scene_size.z >= scene_size.x && scene_size.z >= scene_size.y) {
					r_max = scene_size.z/4;
					sphere1 = { scene_min.x + scene_size.x/2, scene_min.y + scene_size.y/2 , scene_min.z + scene_size.z/4 };
					sphere2 = { scene_min.x + scene_size.x/2, scene_min.y + scene_size.y/2 , scene_max.z - scene_size.z/4 };
				}

				// Setup RNG ()
				int blocks = 200; // max number for MTGP RNG
				int threads = 256; // max number for MTGP RNG
				global_memory_buffer<curandStateMtgp32> rand_state("rand_state", blocks);
				global_memory_buffer<mtgp32_kernel_params> rng_params("rng_params", 1);
				curandMakeMTGP32Constants(mtgp32dc_params_fast_11213, rng_params.device_memory);
				curandMakeMTGP32KernelState(rand_state.device_memory, mtgp32dc_params_fast_11213, rng_params.device_memory, 200, 1234);

				launch_setup_ray_incoherent(int2{blocks,threads},
											resolution,
											rd->rays.device_memory,
											sphere1, sphere2,
											pf->rt->incoherence_r1, pf->rt->incoherence_r2,
											r_max,
											rand_state.device_memory);
				warn_on_cuda_error("");
				potentially_sync_cuda("");
			}
			else {
				rng.compute();

				camera &cam = rc->scene.camera;
				vec3 U = cross(cam.dir, cam.up);
				vec3 V = cross(U, cam.dir);
				launch_setup_rays(U, V, cam.near_w, cam.near_h, resolution,
								  float3{cam.pos.x, cam.pos.y, cam.pos.z},
								  float3{cam.dir.x, cam.dir.y, cam.dir.z},
								  rng.random_numbers, rd->rays.device_memory);
				warn_on_cuda_error("");
				potentially_sync_cuda("");
			}
		}

		void store_hitpoint_albedo_cpu::run() {
			time_this_block(store_hitpoint_albedo_cpu);
			auto res = rc->resolution();

			// this is probably the correct rd, but make sure
			pf->rt->rd->intersections.download();
			::triangle_intersection *is = (::triangle_intersection*)pf->rt->rd->intersections.host_data.data();

			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					vec3 radiance(0);
					::triangle_intersection &hit = is[y*res.x+x];
					// std::cout << "is #" << (y*res.x+x) << " t" << hit.t << " b" << hit.beta << " g" << hit.gamma << " r" << hit.ref << std::endl;
					if (hit.valid()) {
						diff_geom dg(hit, rc->scene);
						radiance += dg.albedo();
					}
					// radiance *= one_over_samples;
					rc->framebuffer.color(x,y) = vec4(radiance, 1);
				}
		}
		
		void add_hitpoint_albedo_to_fb::run() {
			time_this_wf_step;
			auto res = int2{rc->resolution().x, rc->resolution().y};

			launch_add_hitpoint_albedo(res,
									   sample_rays->intersections.device_memory,
									   pf->sd->triangles.device_memory,
									   pf->sd->vertex_tc.device_memory,
									   pf->sd->materials.device_memory,
									   sample_rays->framebuffer.device_memory);
			warn_on_cuda_error("");
			potentially_sync_cuda("");
		}
	
		void initialize_framebuffer::run() {
			time_this_wf_step;
			auto res = int2{rc->resolution().x, rc->resolution().y};

			launch_initialize_framebuffer_data(res, rd->framebuffer.device_memory);
		}
			
		void download_framebuffer::run() {
			time_this_wf_step;
			auto res = int2{rc->resolution().x, rc->resolution().y};
			rd->framebuffer.download();
			float4 *fb = rd->framebuffer.host_data.data();

			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					float4 c = fb[y*res.x+x];
					rc->framebuffer.color(x,y) = vec4(c.x, c.y, c.z, c.w) / c.w;
				}
		}
		
		find_closest_hits::find_closest_hits() : wf::wire::find_closest_hits<raydata>(pf->rt) {
		}

		find_any_hits::find_any_hits() : wf::wire::find_any_hits<raydata>(pf->rt) {
		}

		
	}
}

