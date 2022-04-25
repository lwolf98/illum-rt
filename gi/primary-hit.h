#pragma once

#include "libgi/algorithm.h"
#include "libgi/wavefront-rt.h"
#include "libgi/material.h"

/* \brief Display the color (albedo) of the surface closest to the given ray.
 *
 * - x, y are the pixel coordinates to sample a ray for.
 * - samples is the number of samples to take
 * - render_context holds contextual information for rendering (e.g. a random number generator)
 *
 */
class primary_hit_display : public recursive_algorithm {
public:
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples) override;
};

#ifndef RTGI_SKIP_LOCAL_ILLUM
class local_illumination : public recursive_algorithm {
public:
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples) override;
};
#endif

#ifndef RTGI_SKIP_WF
namespace wf {
	
	struct initialize_framebuffer : public ray_and_intersection_processing { static constexpr char id[] = "initialize framebuffer"; };
	struct sample_camera_rays     : public ray_and_intersection_processing { static constexpr char id[] = "sample camera rays"; };
	struct add_hitpoint_albedo    : public ray_and_intersection_processing { static constexpr char id[] = "add hitpoint albedo"; };
	struct download_framebuffer   : public ray_and_intersection_processing { static constexpr char id[] = "download framebuffer"; };

	class primary_hit_display : public wf::simple_algorithm {
		step *clear, *download;
	public:
		primary_hit_display();
		void prepare_frame() override;
		void compute_samples() override;
		void finalize_frame() override;
	};

}
#endif
