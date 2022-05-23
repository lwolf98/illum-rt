#pragma once

#include "wavefront.h"

#include "libgi/intersect.h"

#ifndef RTGI_SKIP_WF
/*! Here we are inconsistent and use the ::scene instead of wf::cpu::scene
 *  because this is code that is also run for the individual ray tracer.
 *
 *  TODO: will this cause problems?
 */
#endif
struct seq_tri_is : public individual_ray_tracer {
	void build(::scene *scene) { this->scene = scene; }
	triangle_intersection closest_hit(const ray &ray);
	bool any_hit(const ray &ray);
};

