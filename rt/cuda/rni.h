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

		/*! \brief Compute albedo (incl. textures) on the GPU
		 *
		 */
		struct add_hitpoint_albedo_to_fb : public ray_and_intersection_processing {
			bool first_sample;
			void run() override;
		};
		
		/*! \brief Download frame buffer data for further processing on the host
		 *
		 */
		struct download_framebuffer : public ray_and_intersection_processing {
			void run() override;
		};


	}
}
