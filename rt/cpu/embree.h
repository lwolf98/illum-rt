#pragma once

#include "wavefront.h"

#include "libgi/intersect.h"
#include "embree3/rtcore.h"

#include <iostream>

#ifndef RTGI_SKIP_WF
/*! Here we are inconsistent and use the ::scene instead of wf::cpu::scene
 *  because this is code that is also run for the individual ray tracer.
 *
 *  TODO: will this cause problems?
 */
#endif
struct embree_tracer : public individual_ray_tracer {
public:
    embree_tracer();
    ~embree_tracer();
    void build(::scene *scene);
    bool any_hit(const ray &ray);
    triangle_intersection closest_hit(const ray &ray);

private:
    RTCDevice em_device;
    RTCScene em_scene;
};
