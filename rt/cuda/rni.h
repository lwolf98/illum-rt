#pragma once

#include "base.h"

#include "gi/primary-hit.h"

#include <curand.h>

namespace wf {
	namespace cuda {

		/*! \brief Set up camera rays using Shirley's formulas.
		 *
		 */
		class batch_cam_ray_setup : public wf::wire::sample_camera_rays<raydata> {
			curandGenerator_t gen;
			float2 *random_numbers = nullptr;
		public:
			batch_cam_ray_setup();
			~batch_cam_ray_setup();
			void run() override;
		};

		/*! \brief Download hitpoints and compute shading on the host (i.e. the CPU)
		 *
		 * 	Note: Shading should become a separate step to run on the GPU at some point.
		 * 	Note: Only supports computing a single sample right now.
		 *
		 * 	Note: This should be obsolete...
		 */
		struct store_hitpoint_albedo_cpu : public wf::step {
			void run() override;
		};

		/*! \brief Compute albedo (incl. textures) on the GPU
		 *
		 */
		struct add_hitpoint_albedo_to_fb : public wf::wire::add_hitpoint_albedo<raydata> {
			bool first_sample;
			void run() override;
		};
		
		/*! \brief Download frame buffer data for further processing on the host
		 *
		 */
		struct initialize_framebuffer : public wf::wire::initialize_framebuffer<raydata> {
			void run() override;
		};

		/*! \brief Download frame buffer data for further processing on the host
		 *
		 */
		struct download_framebuffer : public wf::wire::download_framebuffer<raydata> {
			void run() override;
		};

		struct find_closest_hits : public wf::wire::find_closest_hits<raydata> {
			find_closest_hits();
		};
			
		struct find_any_hits : public wf::wire::find_any_hits<raydata> {
			find_any_hits();
		};
	
	}
}
