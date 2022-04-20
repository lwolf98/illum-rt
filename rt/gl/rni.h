#pragma once

#include "base.h"
#include "shader.h"

#include "gi/primary-hit.h"

namespace wf {
	namespace gl {
		
		/*! \brief Clear framebuffer data to (0,0,0,0)
		 *
		 */
		struct initialize_framebuffer : public rni<wf::initialize_framebuffer> {
			void run() override;
		};
			
		/*! \brief Copy framebuffer to host memory
		 *
		 */
		struct download_framebuffer : public rni<wf::download_framebuffer> {
			void run() override;
		};
			
		/*! \brief Set up camera rays using Shirley's formulas.
		 *
		 */
		struct batch_cam_ray_setup : public rni<wf::sample_camera_rays> {
			void run() override;
		};
		

		/*! \brief Download hitpoints and compute shading on the host (i.e. the CPU)
		 *
		 * 	Note: Shading should become a separate step to run on the GPU at some point.
		 * 	Note: Only supports computing a single sample right now.
		 */
		struct add_hitpoint_albedo : public rni<wf::add_hitpoint_albedo> {
			void run() override;
		};

	}
}
