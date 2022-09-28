#include "bounce.h"

#include "libgi/util.h"
#include "libgi/sampling.h"

#include <iostream>
using namespace std;

namespace wf::cpu {

	void sample_uniform_dir::run() {
		time_this_wf_step;
		auto res = rc->resolution();
		float pdf = one_over_2pi;
		#pragma omp parallel for
		for (int y = 0; y < res.y; ++y)
			for (int x = 0; x < res.x; ++x) {
				auto is = camdata->intersections[y*res.x+x];
				vec3 radiance(0);
				ray shadow_ray(vec3(0),vec3(1));
				shadow_ray.t_max = -FLT_MAX;
				if (is.valid()) {
					diff_geom hit(is, *pf->sd);
					flip_normals_to_ray(hit, camdata->rays[y*res.x+x]);
					if (hit.mat->emissive != vec3(0)) {
						radiance = hit.mat->emissive;
						rc->framebuffer.color(x,y) += vec4(radiance, 1);
					}
					else {
						vec2 xi = rc->rng.uniform_float2();
						vec3 sampled_dir = uniform_sample_hemisphere(xi);
						vec3 w_i = align(sampled_dir, hit.ng);
						shadow_ray = ray(hit.x, w_i);
					}
					this->pdf->data[y*res.x+x] = pdf;
					bouncedata->rays[y*res.x+x] = shadow_ray;
				}
			}
	}

	void sample_cos_weighted_dir::run() {
		time_this_wf_step;
		auto res = rc->resolution();
		float pdf = one_over_pi;	// changed wrt sample_uniform_dir
		#pragma omp parallel for
		for (int y = 0; y < res.y; ++y)
			for (int x = 0; x < res.x; ++x) {
				auto is = camdata->intersections[y*res.x+x];
				vec3 radiance(0);
				ray shadow_ray(vec3(0),vec3(1));
				shadow_ray.t_max = -FLT_MAX;
				if (is.valid()) {
					diff_geom hit(is, *pf->sd);
					flip_normals_to_ray(hit, camdata->rays[y*res.x+x]);
					if (hit.mat->emissive != vec3(0)) {
						radiance = hit.mat->emissive;
						rc->framebuffer.color(x,y) += vec4(radiance, 1);
					}
					else {
						vec2 xi = rc->rng.uniform_float2();
						// changed wrt sample_uniform_dir
						vec3 sampled_dir = cosine_sample_hemisphere(xi);
						// until here
						vec3 w_i = align(sampled_dir, hit.ng);
						pdf *= cdot(sampled_dir, hit.ng);
						shadow_ray = ray(hit.x, w_i);
					}
					this->pdf->data[y*res.x+x] = pdf;
					bouncedata->rays[y*res.x+x] = shadow_ray;
				}
			}
	}

	void integrate_light_sample::run() {
		time_this_wf_step;
		auto res = rc->resolution();
		#pragma omp parallel for
		for (int y = 0; y < res.y; ++y)
			for (int x = 0; x < res.x; ++x) {
				auto is = camrays->intersections[y*res.x+x];
				auto light_is = shadowrays->intersections[y*res.x+x];
				vec3 radiance(0);
				if (is.valid() && light_is.valid()) {
					diff_geom hit(is, *pf->sd);
					diff_geom light_hit(light_is, *pf->sd);
					flip_normals_to_ray(light_hit, shadowrays->rays[y*res.x+x]);
					float pdf = this->pdf->data[y*res.x+x];
					vec3 w_out = -camrays->rays[y*res.x+x].d;
					vec3 w_in = shadowrays->rays[y*res.x+x].d;
					if (light_hit.mat->emissive != vec3(0)) {
						radiance = light_hit.mat->emissive * hit.mat->brdf->f(hit, w_out, w_in) * cdot(w_in, hit.ns) / pdf;
					}
				}
				rc->framebuffer.color(x,y) += vec4(radiance, 1);
			}
	}

}
