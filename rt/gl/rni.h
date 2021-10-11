#pragma once

#include "base.h"
#include "shader.h"

namespace wf {
	namespace gl {
		
		struct batch_cam_ray_setup : public ray_and_intersection_processing {
			void run() override;
		};
		
		struct store_hitpoint_albedo : public ray_and_intersection_processing {
			void run() override;
		};

	}
}
