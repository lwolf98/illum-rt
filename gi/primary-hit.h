#pragma once

#include "libgi/algorithm.h"
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

#ifndef RTGI_A02
class local_illumination : public recursive_algorithm {
public:
	gi_algorithm::sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples) override;
};
#endif

class primary_hit_display_wf : public wavefront_algorithm {
};
