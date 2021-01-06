#include "pt.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"
#include "libgi/sampling.h"

#include "libgi/timer.h"

using namespace glm;
using namespace std;

// 
// ----------------------- simple pt -----------------------
//
	
gi_algorithm::sample_result simple_pt::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &r) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		result.push_back({path(cam_ray(rc.scene.camera, x, y, glm::vec2(rc.rng.uniform_float()-0.5f, rc.rng.uniform_float()-0.5f))),
						  vec2(0)});
	}
	return result;
}

vec3 simple_pt::path(ray ray) {
	time_this_block(pathtrace);
	vec3 radiance(0);
	vec3 throughput(1);
	for (int i = 0; i < max_path_len; ++i) {
		
		// find hitpoint with scene
		triangle_intersection closest = rc.scene.rt->closest_hit(ray);
		if (!closest.valid())
			break;
		diff_geom hit(closest, rc.scene);
		
		// if it is a light, add the light's contribution
		if (hit.mat->emissive != vec3(0)) {
			radiance = throughput * hit.mat->emissive;
			break;
		}
		
		// bounce the ray
		auto [bounced,pdf] = bounce_ray(hit, ray);
		throughput *= hit.mat->brdf->f(hit, -ray.d, bounced.d) * cdot(bounced.d, hit.ns) / pdf;
		ray = bounced;
		
		// apply RR
		if (i > rr_start) {
			float xi = uniform_float();
			float p_term = 1.0f - luma(throughput);
			if (xi > p_term)
				throughput *= 1.0f/(1.0f-p_term);
			else
				break;
		}
	}
	return radiance;
}

std::tuple<ray,float> simple_pt::bounce_ray(const diff_geom &hit, const ray &to_hit) {
	if (bounce == bounce::uniform)
		return sample_uniform_direction(hit);
	else if (bounce == bounce::cosine)
		return sample_cosine_distributed_direction(hit);
	else
		return sample_brdf_distributed_direction(hit, to_hit);
}

bool simple_pt::interprete(const std::string &command, std::istringstream &in) {
	string sub, val;
	if (command == "path") {
		in >> sub;
		if (sub == "len") {
			int i = 0;
			in >> i;
			if (i <= 0)
				cerr << "error in path len: expected a positive integer, got " << i << endl;
			else
				max_path_len = i;
			return true;
		}
		else if (sub == "bounce") {
			in >> val;
			if (val == "uniform")     bounce = bounce::uniform;
			else if (val == "cosine") bounce = bounce::cosine;
			else if (val == "brdf")   bounce = bounce::brdf;
			else cerr << "error: invalid kind of path bounce: '" << val << "'" << endl;
			return true;
		}
		else if (sub == "rr-start") {
			int i = 0;
			in >> i;
			if (i <= 0)
				cerr << "error in russian roulette offset: expected a positive integer > 0, got " << i << endl;
			else
				rr_start = i;
			return true;
		}
		else {
			cerr << "unknown subcommand to path: '" << sub << "'" << endl;
			return true;
		}
	}
	return false;
}

// 
// ----------------------- pt with next event estimation -----------------------
//

vec3 pt_nee::path(ray ray) {
	time_this_block(pathtrace);
	vec3 radiance(0);
	vec3 throughput(1);
	for (int i = 0; i < max_path_len; ++i) {
		
		// find hitpoint with scene
		triangle_intersection closest = rc.scene.rt->closest_hit(ray);
		if (!closest.valid())
			break;
		diff_geom hit(closest, rc.scene);

		// if it is a light AND we have not bounced yet, add the light's contribution
		if (i == 0 && hit.mat->emissive != vec3(0)) {
			radiance = throughput * hit.mat->emissive;
			break;
		}

		// branch off direct lighting path that directly terminates
		auto [shadow_ray,light_col,light_pdf] = sample_light(hit);
		if (light_pdf != 0 && light_col != vec3(0))
			if (!rc.scene.rt->any_hit(shadow_ray))
				radiance += throughput * light_col * hit.mat->brdf->f(hit, -ray.d, shadow_ray.d) * cdot(shadow_ray.d, hit.ns) / light_pdf;

		// bounce the ray
		auto [bounced,pdf] = bounce_ray(hit, ray);
		throughput *= hit.mat->brdf->f(hit, -ray.d, bounced.d) * cdot(bounced.d, hit.ns) / pdf;
		ray = bounced;

		// apply RR
		if (i > rr_start) {
			float xi = uniform_float();
			float p_term = 1.0f - luma(throughput);
			if (xi > p_term)
				throughput *= 1.0f/(1.0f-p_term);
			else
				break;
		}
	}
	return radiance;
}

std::tuple<ray,vec3,float> pt_nee::sample_light(const diff_geom &hit) {
	auto [l_id, l_pdf] = rc.scene.light_distribution->sample_index(rc.rng.uniform_float());
	light *l = rc.scene.lights[l_id];
	auto [shadow_ray,l_col,pdf] = l->sample_Li(hit, rc.rng.uniform_float2());
	return { shadow_ray, l_col, pdf * l_pdf };
}

bool pt_nee::interprete(const std::string &command, std::istringstream &in) {
	if (simple_pt::interprete(command, in)) 
		return true;

	return false;
}
