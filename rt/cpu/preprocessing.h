#pragma once

#include "platform.h"
#include "wavefront.h"

/* This file holds wavefront-steps that can be run when the scene is committed to the platform.
 * These steps could also make sense in a non-wavefront algorithm.
 *
 */

namespace wf::cpu {

	struct build_accel_struct : public wf::build_accel_struct {
		void run() override;
	};

}
