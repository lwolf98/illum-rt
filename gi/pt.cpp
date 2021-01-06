#include "pt.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"
#include "libgi/sampling.h"

using namespace glm;
using namespace std;

	
gi_algorithm::sample_result simple_pt::sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &r) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		result.push_back({path(cam_ray(rc.scene.camera, x, y, glm::vec2(rc.rng.uniform_float()-0.5f, rc.rng.uniform_float()-0.5f))),
						  vec2(0)});
	}
	return result;
}

vec3 simple_pt::path(ray ray) {
	vec3 radiance(0);
	vec3 throughput(1);
	for (int i = 0; i < max_path_len; ++i) {
		triangle_intersection closest = rc.scene.rt->closest_hit(ray);
		if (!closest.valid())
			break;
		diff_geom dg(closest, rc.scene);
		if (dg.mat->emissive != vec3(0)) {
			radiance = throughput * dg.mat->emissive;
			break;
		}
		auto [bounced,pdf] = bounce_ray(dg, ray);
		throughput *= dg.mat->brdf->f(dg, -ray.d, bounced.d) * cdot(bounced.d, dg.ns) / pdf;
		ray = bounced;
	}
	return radiance;
}

std::tuple<ray,float> simple_pt::bounce_ray(const diff_geom &dg, const ray &to_hit) {
	if (bounce == bounce::uniform)
		return sample_uniform_direction(dg);
	else if (bounce == bounce::cosine)
		return sample_cosine_distributed_direction(dg);
	else
		return sample_brdf_distributed_direction(dg, to_hit);
}

bool simple_pt::interprete(const std::string &command, std::istringstream &in) {
	string sub, val;
	if (command == "path") {
		in >> sub;
		if (sub == "len") {
			int i = 0;
			in >> i;
			if (i<=0)
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
		else {
			cerr << "unknown subcommand to path: '" << sub << "'" << endl;
			return true;
		}
	}
	return false;
}

