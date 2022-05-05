#pragma once

#include "base.h"
#include "shader.h"

#include "gi/primary-hit.h"

namespace wf {
	namespace gl {
		
		/*! \brief Clear framebuffer data to (0,0,0,0)
		 *
		 */
		struct initialize_framebuffer : public wf::initialize_framebuffer {
			initialize_framebuffer();
			void run() override;
		};
			
		/*! \brief Copy framebuffer to host memory
		 *
		 */
		struct download_framebuffer : public wf::download_framebuffer {
			void run() override;
		};
			
		/*! \brief Set up camera rays using Shirley's formulas.
		 *
		 */
		class batch_cam_ray_setup : public wf::sample_camera_rays {
			ssbo<uint64_t> pcg_data;
			void init_pcg_data(int w, int h);
		public:
			batch_cam_ray_setup();
			~batch_cam_ray_setup();
			void run() override;
		};
		
		struct find_closest_hits : public wf::find_closest_hits {
			find_closest_hits();
		};
			
		struct find_any_hits : public wf::find_any_hits {
			find_any_hits();
		};
			
		/*! \brief Download hitpoints and compute shading on the host (i.e. the CPU)
		 *
		 * 	Note: Shading should become a separate step to run on the GPU at some point.
		 * 	Note: Only supports computing a single sample right now.
		 */
		class add_hitpoint_albedo : public wf::add_hitpoint_albedo {
			compute_shader *cs;
		public:
			add_hitpoint_albedo();
			void run() override;
		};

	}
}
