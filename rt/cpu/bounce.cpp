#include "bounce.h"

#include "libgi/util.h"

namespace wf::cpu {

	void sample_uniform_light_directions::run() {
		int w = sample_rays->w;
		int h = sample_rays->h;
		#pragma omp parallel for
		for (int y = 0; y < w; ++y)
			for (int x = 0; x < h; ++x) {
				auto is = sample_rays->intersections[y*w+x];
				vec3 radiance(0);
				ray shadow_ray(vec3(0),vec3(1));
				shadow_ray.t_max = -FLT_MAX;
				if (is.valid()) {
					diff_geom dg(is, *pf->sd);
					flip_normals_to_ray(dg, sample_rays->rays[y*w+x]);
					if (dg.mat->emissive != vec3(0)) {
						radiance = dg.mat->emissive;
					}
					else {
						vec2 xi = rc->rng.uniform_float2();
						float z = xi.x;
						float phi = 2*pi*xi.y;
						// z is cos(theta), sin(theta) = sqrt(1-cos(theta)^2)
						float sin_theta = sqrtf(1.0f - z*z);
						vec3 sampled_dir = vec3(sin_theta * cosf(phi),
												sin_theta * sinf(phi),
												z);
						vec3 w_i = align(sampled_dir, dg.ng);
						shadow_ray = ray(dg.x, w_i);
					}
					rc->framebuffer.color(x,y) += vec4(radiance, 1);
					shadow_rays->rays[y*w+x] = shadow_ray;
				}
			}
	}

}
