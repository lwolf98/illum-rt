#include "denoise.h"
#include "libgi/denoise.h"
#include "libgi/util.h"

#include <iostream>
using namespace std;

namespace wf::cpu {
    
    void add_hitpoint_albedo_to_framebuffer::run() {
        if (rc->enable_denoising) {
            time_this_wf_step;
            auto res = rc->resolution();
            #pragma omp parallel for
            for (int y = 0; y < res.y; ++y)
                for (int x = 0; x < res.x; ++x) {
                    vec3 radiance(0);
                    triangle_intersection closest = sample_rays->intersections[y*res.x+x];
                    if (closest.valid()) {
                        diff_geom dg(closest, *pf->sd);
                        if (dg.mat->emissive != vec3(0))
                            radiance += dg.mat->emissive;
                        else
                            radiance += dg.albedo();
                    }
                    rc->framebuffer_albedo.color.data[y*res.x+x] += vec4(radiance, 1.0f);
                }
        }
    }

    void add_hitpoint_normal_to_framebuffer::run() {
        if (rc->enable_denoising) {
            time_this_wf_step;
            auto res = rc->resolution();
            #pragma omp parallel for
            for (int y = 0; y < res.y; ++y)
                for (int x = 0; x < res.x; ++x) {
                    vec3 radiance(0);
                    triangle_intersection closest = sample_rays->intersections[y*res.x+x];
                    if (closest.valid()) {
                        diff_geom dg(closest, *pf->sd);
                        flip_normals_to_ray(dg, sample_rays->rays[y*res.x+x]);
                        radiance += dg.ns;
                    }
                    rc->framebuffer_normal.color.data[y*res.x+x] += vec4(radiance, 1.0f);
                }
        }
    }
}
