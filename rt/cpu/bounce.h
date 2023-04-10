#pragma once

#include "platform.h"
#include "wavefront.h"
#include "preprocessing.h"

/* 
 * This file contains wf steps that sample path extenstions
 *
 */

namespace wf::cpu {

	struct sample_uniform_dir : public wf::wire::sample_uniform_dir<raydata, per_sample_data<float>> {
		void run() override;
	};

	struct sample_cos_weighted_dir : public wf::wire::sample_cos_weighted_dir<raydata, per_sample_data<float>> {
		void run() override;
	};

	struct sample_light_dir : public wf::wire::sample_light_dir<raydata, per_sample_data<float>, compute_light_distribution, per_sample_data<vec3>> {
		void run() override;
	};

	struct integrate_dir_sample : public wf::wire::integrate_dir_sample<raydata, per_sample_data<float>> {
		void run() override;
	};

	struct integrate_light_sample : public wf::wire::integrate_light_sample<raydata, per_sample_data<float>, per_sample_data<vec3>> {
		void run() override;
	};

}
