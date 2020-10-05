#pragma once

#include "libgi/scene.h"
#include "libgi/intersect.h"

struct seq_tri_is : public ray_tracer {
	void build(::scene *scene) { this->scene = scene; }
	triangle_intersection closest_hit(const ray &ray);
	bool any_hit(const ray &ray);
};


