#pragma once

#include "libgi/scene.h"
#include "libgi/intersect.h"
#include "embree3/rtcore.h"

#include <iostream>

struct embree_tracer : public individual_ray_tracer {
public:
    embree_tracer();
    ~embree_tracer();
    void build(::scene *);
    bool any_hit(const ray &ray);
    triangle_intersection closest_hit(const ray &ray);

private:
    RTCDevice em_device;
    RTCScene em_scene;
};