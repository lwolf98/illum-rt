#include "primary-hit.h"

#include "rt.h"
#include "context.h"
#include "intersect.h"
#include "util.h"

using namespace glm;
using namespace std;

gi_algorithm::sample_result primary_hit_display::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &rc) {
	sample_result result;
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
	return result;
}


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
				//auto col = dg.mat->albedo_tex ? dg.mat->albedo_tex->sample(dg.tc) : dg.mat->albedo;
				if (sampling_mode == sample_light) {
					auto [l_id, l_pdf] = rc.scene.light_distribution->sample_index(rc.rng.uniform_float());
					light *l = rc.scene.lights[l_id];
					auto [shadow_ray,col,pdf] = l->sample_Li(dg, rc.rng.uniform_float2());
					if (auto is = rc.scene.rt->closest_hit(shadow_ray); !is.valid() || is.t > shadow_ray.t_max) {
						radiance = col * brdf->f(dg, -view_ray.d, shadow_ray.d) * cdot(shadow_ray.d, dg.ns) / (pdf * l_pdf);
					}
				}
				else {
					auto [w_i, f, pdf] = brdf->sample(dg, -view_ray.d, rc.rng.uniform_float2());
					ray light_ray(nextafter(dg.x, w_i), w_i);
					if (auto is = rc.scene.rt->closest_hit(light_ray); is.valid())
						if (diff_geom hit_geom(is, rc.scene); hit_geom.mat->emissive != vec3(0))
							radiance = f * hit_geom.mat->emissive * cdot(dg.ns, w_i) / pdf;
				}
			}
		}
		result.push_back({radiance,vec2(0)});
	}
	return result;
}
	
bool direct_light::interprete(const std::string &command, std::istringstream &in) {
	string value;
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
	else if (command == "is") {
		in >> value;
		if (value == "light") sampling_mode = sample_light;
		else if (value == "brdf") sampling_mode = sample_brdf;
		else cerr << "unknown sampling mode in " << __func__ << ": " << value << endl;
		return true;
	}
	return false;
}

