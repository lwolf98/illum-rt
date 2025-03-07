#pragma once

#include "libgi/scene.h"
#include "libgi/intersect.h"
#include "libgi/subdivision.h"

#include <vector>
#include <float.h>
#include <glm/glm.hpp>

#ifndef RTGI_SKIP_WF
/*! Here we are inconsistent and use the ::scene instead of wf::cpu::scene
 *  because this is code that is also run for the individual ray tracer.
 *
 *  TODO: will this cause problems?
 */
#endif
struct subd_naive_bvh : public individual_ray_tracer {
	

	std::vector<subd::node> nodes;
	uint32_t root;
	bool debug;
	void build(::scene *scene);
private:
	uint32_t subdivide(std::vector<triangle> &triangles, std::vector<vertex> &vertices, uint32_t start, uint32_t end);
	triangle_intersection closest_hit(const ray &ray) override;
	bool any_hit(const ray &ray) override;
};