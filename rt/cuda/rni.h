#pragma once

#include "base.h"

#include "gi/primary-hit.h"

#include <curand.h>

namespace wf {
	namespace cuda {

		/*! \brief Set up camera rays using Shirley's formulas.
		 *
		 */
		class batch_cam_ray_setup : public wf::sample_camera_rays {
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
		struct add_hitpoint_albedo_to_fb : public wf::add_hitpoint_albedo {
			bool first_sample;
			void run() override;
		};
		
		/*! \brief Download frame buffer data for further processing on the host
		 *
		 */
		struct initialize_framebuffer : public wf::initialize_framebuffer {
			void run() override;
		};

		/*! \brief Download frame buffer data for further processing on the host
		 *
		 */
		struct download_framebuffer : public wf::download_framebuffer {
			void run() override;
		};

		struct find_closest_hits : public wf::find_closest_hits {
			find_closest_hits();
		};
			
		struct find_any_hits : public wf::find_any_hits {
			find_any_hits();
		};
	
	}
}
