#pragma once

#include "platform.h"
#include "wavefront.h"

/* 
 * This file contains wf steps that sample path extenstions
 *
 */

namespace wf::cpu {

	struct sample_uniform_light_directions : public wf::sample_uniform_light_directions {
		raydata *sample_rays = nullptr,
		        *shadow_rays = nullptr;
		void run() override;
	};

}
