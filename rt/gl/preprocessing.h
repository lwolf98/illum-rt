#pragma once

#include "platform.h"
#include "base.h"
#include "gi/direct-steps.h"

/* This file holds wavefront-steps that can be run when the scene is committed to the platform.
 * These steps could also make sense in a non-wavefront algorithm.
 *
 */

namespace wf::gl {

	struct build_accel_struct : public wf::build_accel_struct {
		void run() override;
	};

	/*! The GL implementation of this step currently runs on the CPU and only copies over the data.
	 *  That is, in animated scenes this will be slow.
	 *  Todo this it relies on \c rc->scene
	 *  TODO: compute on GPU :)
	 */
	struct compute_light_distribution : public wf::compute_light_distribution {
		compute_light_distribution();
		data_texture<float> f, cdf;
		float integral_1spaced;
		int n;
		data_texture<ivec4> tri_lights;
		void run() override;
	};

}
