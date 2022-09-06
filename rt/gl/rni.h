#pragma once

#include "base.h"
#include "shader.h"

#include "gi/primary-hit.h"

namespace wf {
	namespace gl {
		
		/*! \brief Clear framebuffer data to (0,0,0,0)
		 *
		 */
		struct initialize_framebuffer : public wf::wire::initialize_framebuffer<raydata> {
			initialize_framebuffer();
			void run() override;
		};
			
		/*! \brief Copy framebuffer to host memory
		 *
		 */
		struct download_framebuffer : public wf::wire::download_framebuffer<raydata> {
			void run() override;
		};
			
		/*! \brief Set up camera rays using Shirley's formulas.
		 *
		 */
		class batch_cam_ray_setup : public wf::wire::sample_camera_rays<raydata> {
			gl::rng rng;
		public:
			batch_cam_ray_setup();
			~batch_cam_ray_setup();
			void run() override;
		};
		
		struct find_closest_hits : public wf::wire::find_closest_hits<raydata> {
			find_closest_hits();
		};
			
		struct find_any_hits : public wf::wire::find_any_hits<raydata> {
			find_any_hits();
		};
			
		/*! \brief Download hitpoints and compute shading on the host (i.e. the CPU)
		 *
		 * 	Note: Shading should become a separate step to run on the GPU at some point.
		 * 	Note: Only supports computing a single sample right now.
		 */
		class add_hitpoint_albedo : public wf::wire::add_hitpoint_albedo<raydata> {
			compute_shader *cs;
		public:
			add_hitpoint_albedo();
			void run() override;
		};

	}
}
