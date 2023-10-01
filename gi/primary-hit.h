#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"

#ifndef RTGI_SKIP_WF
#include "libgi/wavefront-rt.h"
#endif

/* \brief Display the color (albedo) of the surface closest to the given ray.
 *
 * - x, y are the pixel coordinates to sample a ray for.
 * - samples is the number of samples to take
 * - render_context holds contextual information for rendering (e.g. a random number generator)
 *
 */
class primary_hit_display : public recursive_algorithm {
public:
	glm::vec3 sample_pixel(uint32_t x, uint32_t y) override;
};

#ifndef RTGI_SKIP_LOCAL_ILLUM
class local_illumination : public recursive_algorithm {
public:
	glm::vec3 sample_pixel(uint32_t x, uint32_t y) override;
};
#endif

#ifndef RTGI_SKIP_DEBUGALGO
class info_display : public recursive_algorithm {
public:
	glm::vec3 sample_pixel(uint32_t x, uint32_t y) override;
};
#endif

#ifndef RTGI_SKIP_WF
#include "gi/primary-steps.h"
namespace wf {
	class primary_hit_display : public simple_algorithm  {
		raydata *rd = nullptr;
	public:
		primary_hit_display();
	};
}
#endif
