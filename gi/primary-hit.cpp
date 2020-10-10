#include "primary-hit.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"

using namespace glm;
using namespace std;

gi_algorithm::sample_result primary_hit_display::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &rc) {
	sample_result result;
#ifndef RTGI_A01
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0);
		ray view_ray = cam_ray(rc.scene.camera, x, y, glm::vec2(rc.rng.uniform_float()-0.5f, rc.rng.uniform_float()-0.5f));
		triangle_intersection closest = rc.scene.rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc.scene);
			radiance = dg.albedo();
		}
		result.push_back({radiance,vec2(0)});
	}
#else
	result.push_back({vec3(0),vec2(0)});
#endif
	return result;
}

#ifndef RTGI_A02
gi_algorithm::sample_result local_illumination::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &rc) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0);
		ray view_ray = cam_ray(rc.scene.camera, x, y, glm::vec2(rc.rng.uniform_float()-0.5f, rc.rng.uniform_float()-0.5f));
		triangle_intersection closest = rc.scene.rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc.scene);
			brdf *brdf = dg.mat->brdf;
			assert(!rc.scene.lights.empty());
			pointlight *pl = dynamic_cast<pointlight*>(rc.scene.lights[0]);
			assert(pl);
#ifndef RTGI_A03
			vec3 to_light = pl->pos - dg.x;
			vec3 w_i = normalize(to_light);
			vec3 w_o = -view_ray.d;
			float d = sqrtf(dot(to_light,to_light));

			ray shadow_ray(dg.x, w_i);
			shadow_ray.length_exclusive(d);
			if (!rc.scene.rt->any_hit(shadow_ray))
				radiance = pl->power() * brdf->f(dg, w_o, w_i) / (d*d);
#else
			// todo
			radiance = dg.albedo();
#endif
		}
		result.push_back({radiance,vec2(0)});
	}
	return result;
}
#endif


#ifndef RTGI_AXX
gi_algorithm::sample_result direct_light::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &rc) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0);
		ray view_ray = cam_ray(rc.scene.camera, x, y, glm::vec2(rc.rng.uniform_float()-0.5f, rc.rng.uniform_float()-0.5f));
		triangle_intersection closest = rc.scene.rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc.scene);
			if (dg.mat->emissive != vec3(0)) {
				radiance = dg.mat->emissive;
			}
			else {
				brdf *brdf = dg.mat->brdf;
				//auto col = dg.mat->albedo_tex ? dg.mat->albedo_tex->sample(dg.tc) : dg.mat->albedo;
				if (sampling_mode == sample_light) {
					auto [l_id, l_pdf] = rc.scene.light_distribution->sample_index(rc.rng.uniform_float());
					light *l = rc.scene.lights[l_id];
					auto [shadow_ray,col,pdf] = l->sample_Li(dg, rc.rng.uniform_float2());
					if (col != vec3(0))
						if (!rc.scene.rt->any_hit(shadow_ray))
							// col already in brdf. inconsistent?
							radiance = col * brdf->f(dg, -view_ray.d, shadow_ray.d) * cdot(shadow_ray.d, dg.ns) / (pdf * l_pdf);
				}
				else if (sampling_mode == sample_brdf) {
					auto [w_i, f, pdf] = brdf->sample(dg, -view_ray.d, rc.rng.uniform_float2());
					ray light_ray(nextafter(dg.x, w_i), w_i);
					if (auto is = rc.scene.rt->closest_hit(light_ray); is.valid())
						if (diff_geom hit_geom(is, rc.scene); hit_geom.mat->emissive != vec3(0))
							radiance = f * hit_geom.mat->emissive * cdot(dg.ns, w_i) / pdf;
				}
				else {
					float pdf_light = 0,
						  pdf_brdf = 0;
					if (sample < samples/2-1) {
						auto [l_id, l_pdf] = rc.scene.light_distribution->sample_index(rc.rng.uniform_float());
						light *l = rc.scene.lights[l_id];
						auto [shadow_ray,col,pdf] = l->sample_Li(dg, rc.rng.uniform_float2());
						pdf_light = l_pdf*pdf;
						pdf_brdf  = brdf->pdf(dg, -view_ray.d, shadow_ray.d);
						if (col != vec3(0))
							if (auto is = rc.scene.rt->closest_hit(shadow_ray); !is.valid() || is.t > shadow_ray.t_max)
								radiance = col * brdf->f(dg, -view_ray.d, shadow_ray.d) * cdot(shadow_ray.d, dg.ns);
					}
					else {
						auto [w_i, f, pdf] = brdf->sample(dg, -view_ray.d, rc.rng.uniform_float2());
						ray light_ray(nextafter(dg.x, w_i), w_i);
						pdf_brdf  = pdf;
						if (f != vec3(0))
							if (auto is = rc.scene.rt->closest_hit(light_ray); is.valid())
								if (diff_geom hit_geom(is, rc.scene); hit_geom.mat->emissive != vec3(0)) {
									trianglelight tl(rc.scene, is.ref);
									pdf_light = luma(tl.power()) / rc.scene.light_distribution->integral();
									pdf_light *= tl.pdf(light_ray, hit_geom);
									radiance = f * hit_geom.mat->emissive * cdot(dg.ns, w_i);
								}
					}
					assert(pdf_light >= 0);
					assert(pdf_brdf >= 0);
					assert(radiance.x >= 0);assert(std::isfinite(radiance.x));
					assert(radiance.y >= 0);assert(std::isfinite(radiance.y));
					assert(radiance.z >= 0);assert(std::isfinite(radiance.z));
					assert(std::isfinite(pdf_light));
					assert(std::isfinite(pdf_brdf));
					float balance = pdf_light + pdf_brdf; // 1920/229
					if (balance != 0.0f)
						radiance /= balance*0.5;
					else {
						// do another round as this was useless
						sample--;
						continue;
					}
				}
			}
		}
		result.push_back({radiance,vec2(0)});
	}
	return result;
}

bool direct_light::interprete(const std::string &command, std::istringstream &in) {
	string value;
	/*
	if (command == "brdf") {
		in >> value;
		if (value == "lambertian")         brdf = &d_brdf;
		else if (value == "specular")      brdf = &s_brdf;
		else if (value == "layered-phong") brdf = &l_brdf;
		else if (value == "gtr2")          brdf = &gtr2_brdf;
		else if (value == "layered-gtr2")  brdf = &l_brdf2;
		else cerr << "unknown brdf in " << __func__ << ": " << value << endl;
		return true;
	}
	else */if (command == "is") {
		in >> value;
		if (value == "light") sampling_mode = sample_light;
		else if (value == "brdf") sampling_mode = sample_brdf;
		else if (value == "mis") sampling_mode = both;
		else cerr << "unknown sampling mode in " << __func__ << ": " << value << endl;
		return true;
	}
	return false;
}
#endif
