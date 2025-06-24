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
	std::vector<subd::base_node> nodes;
	uint32_t root;
	bool debug = false;
	void build(::scene *scene);
private:
	uint32_t subdivide(std::vector<triangle> &triangles, std::vector<vertex> &vertices, uint32_t start, uint32_t end);
	triangle_intersection closest_hit(const ray &ray) override;
	bool any_hit(const ray &ray) override;
	void traverse_patch(const ray &ray, uint32_t patch_ref, triangle_intersection &closest);
};