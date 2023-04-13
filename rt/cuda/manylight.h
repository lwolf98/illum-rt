#pragma once

#include "platform.h"
#include "base.h"
#include "rng.h"
#include "preprocessing.h"

//#include "gi/direct-steps.h"
#include "gi/manylight-steps.h"
#include "gi/manylight.h"

#include <curand.h>

/* 
 * This file contains wf steps for the manylight algorithm
 *
 */

namespace wf::cuda {

	struct sample_v_0s : public wf::wire::sample_v_0s<raydata, per_sample_data<vec3>, per_sample_data<int>, compute_light_distribution> {
		random_number_generator<float> rng_light;
		random_number_generator<float4> rng_dir;
		void run() override;
	};

	struct create_vpls : public wf::wire::create_vpls<raydata, per_sample_data<vec3>, vpldata, per_sample_data<int>> {
		void run() override;
	};

	struct russian_roulette : public wf::wire::russian_roulette<raydata, per_sample_data<vec3>> {
		random_number_generator<float> rng;
		void run() override;
	};

	struct sample_next_vpls : public wf::wire::sample_next_vpls<raydata, per_sample_data<vec3>, vpldata, per_sample_data<int>> {
		random_number_generator<float2> rng;
		void run() override;
	};

	struct copy_vpls : public wf::wire::copy_vpls<vpldata, per_sample_data<int>> {
		void run() override;
	};

	struct sample_vpls : public wf::wire::sample_vpls<raydata, vpldata, per_sample_data<int>> {
		random_number_generator<float> rng;
		void run() override;
	};

	struct integrate_vpl_samples : public wf::wire::integrate_vpl_samples<raydata, vpldata> {
		void run() override;
	};
	
}