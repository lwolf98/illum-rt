#pragma once

#include "cuda-helpers.h"

#include <curand_kernel.h>
#include <curand_mtgp32_host.h>     // include MTGP host helper functions */
#include <curand_mtgp32dc_p_11213.h>// include MTGP pre-computed parameter sets */

#define CURAND_CALL(x) do { if((x)!=CURAND_STATUS_SUCCESS) { printf("Error at %s:%d\n",__FILE__,__LINE__); exit(99); }} while(0)

namespace wf {
	namespace cuda {

		/*! Generate uniformly random floating point vectors
		 */
		template<typename T> struct random_number_generator {
			static_assert(std::is_same_v<float,T> || std::is_same_v<float2,T> || std::is_same_v<float3,T> || std::is_same_v<float4,T>, "invalid rng type");
			curandGenerator_t gen;
			T *random_numbers = nullptr;
			random_number_generator(int seed = 2022) {
				CURAND_CALL(curandCreateGenerator(&gen, CURAND_RNG_PSEUDO_DEFAULT));
				CURAND_CALL(curandSetPseudoRandomGeneratorSeed(gen, 2022));
				rc->call_at_resolution_change[this] = [this](int w, int h) {
					CHECK_CUDA_ERROR(cudaFree(random_numbers), "");
					CHECK_CUDA_ERROR(cudaMalloc((void**)&random_numbers, w*h*sizeof(T)), "");
				};
				int2 resolution{rc->resolution().x, rc->resolution().y};
				if (resolution.x > 0 && resolution.y > 0)
					CHECK_CUDA_ERROR(cudaMalloc((void**)&random_numbers, resolution.x*resolution.y*sizeof(T)), "");
			}
			~random_number_generator() {
				rc->call_at_resolution_change.erase(this);
				CHECK_CUDA_ERROR(cudaFree(random_numbers), "");
				CURAND_CALL(curandDestroyGenerator(gen));
			}
			void compute() {
				auto resolution = rc->resolution();
				CURAND_CALL(curandGenerateUniform(gen, (float*)random_numbers, resolution.x*resolution.y*sizeof(T)/sizeof(float)));
			}
		};

	}
}

#undef CURAND_CALL
