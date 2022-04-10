#pragma once

#include "base.h"

namespace wf {
	namespace cuda {

		/*! \brief Set up camera rays using Shirley's formulas.
		 *
		 */
		struct batch_cam_ray_setup : public ray_and_intersection_processing {
			void run() override;
		};

		/*! \brief Download hitpoints and compute shading on the host (i.e. the CPU)
		 *
		 * 	Note: Shading should become a separate step to run on the GPU at some point.
		 * 	Note: Only supports computing a single sample right now.
		 */
		struct store_hitpoint_albedo_cpu : public ray_and_intersection_processing {
			void run() override;
		};

		/*! \brief Compute shading and then download framebuffer to the host (i.e. the CPU)
		 *
		 */
		struct store_hitpoint_albedo : public ray_and_intersection_processing {
			void run() override;
		};

	}
}
