#pragma once

#include "platform.h"
#include "base.h"
#include "rng.h"

#include "gi/direct-steps.h"

#include <curand.h>

/* 
 * This file contains wf steps that sample path extenstions
 *
 */

namespace wf::cuda {
	
	struct sample_uniform_dir : public wf::wire::sample_uniform_dir<raydata, per_sample_data<float>> {
		random_number_generator<float2> rng;
		void run() override;
	};

	struct integrate_light_sample : public wf::wire::integrate_light_sample<raydata, per_sample_data<float>> {
		void run() override;
	};


}
