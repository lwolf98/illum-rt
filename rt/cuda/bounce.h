#pragma once

#include "platform.h"
#include "base.h"
#include "rng.h"
#include "preprocessing.h"

#include "gi/direct-steps.h"

#include <curand.h>

/* 
 * This file contains wf steps that sample path extenstions
 *
 */

/*namespace wf::cuda::k {
	static __device__ bool not_black(float4 c);
}
static __device__ bool wf::cuda::k::not_black(float4 c);*/

namespace wf::cuda {
	/*const float eps = 1e-4f; // see rt.h
	int2 frame_res();
	__device__ float3 f3(const float4 &v);
	__device__ float3 hit_ng(const tri_is &hit, const uint4 &tri, const float4 *vert_norm);

	namespace k {
		//static __device__ bool not_black(float4 c);
		//__device__ bool test_funct();
		__device__ int lower_bound(int n, float v, float *lights_cdf);
	}*/
	
	struct sample_uniform_dir : public wf::wire::sample_uniform_dir<raydata, per_sample_data<float>> {
		random_number_generator<float2> rng;
		void run() override;
	};

	struct sample_cos_weighted_dir : public wf::wire::sample_cos_weighted_dir<raydata, per_sample_data<float>> {
		random_number_generator<float2> rng;
		void run() override;
	};

	struct sample_light_dir : public wf::wire::sample_light_dir<raydata, per_sample_data<float>, compute_light_distribution, per_sample_data<vec3>> {
		random_number_generator<float4> rng;
		void run() override;
	};

	struct integrate_dir_sample : public wf::wire::integrate_dir_sample<raydata, per_sample_data<float>> {
		void run() override;
	};

	struct integrate_light_sample : public wf::wire::integrate_light_sample<raydata, per_sample_data<float>, per_sample_data<vec3>> {
		void run() override;
	};



	struct sample_mis_dir : public wf::wire::sample_mis_dir<raydata, per_sample_data<float>, compute_light_distribution, per_sample_data<vec3>> {
		random_number_generator<float4> rng;
		void run() override;
	};

	struct integrate_mis_sample : public wf::wire::integrate_mis_sample<raydata, per_sample_data<float>, compute_light_distribution, per_sample_data<vec3>> {
		void run() override;
	};



}
