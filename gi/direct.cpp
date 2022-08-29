#include "direct.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"
#include "libgi/sampling.h"

#include "libgi/global-context.h"

#include "libgi/wavefront-rt.h"

using namespace glm;
using namespace std;

#ifndef RTGI_SKIP_DIRECT_ILLUM
gi_algorithm::sample_result direct_light::sample_pixel(uint32_t x, uint32_t y, uint32_t samples) {
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0,0,0);
		ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
		triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc->scene);
			flip_normals_to_ray(dg, view_ray);

#ifndef RTGI_SKIP_DIRECT_ILLUM_IMPL
			if (dg.mat->emissive != vec3(0)) {
				radiance = dg.mat->emissive;
			}
			else {
				brdf *brdf = dg.mat->brdf;
				//auto col = dg.mat->albedo_tex ? dg.mat->albedo_tex->sample(dg.tc) : dg.mat->albedo;
				if      (sampling_mode == sample_uniform)   radiance = sample_uniformly(dg, view_ray);
				else if (sampling_mode == sample_light)     radiance = sample_lights(dg, view_ray);
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
				else if (sampling_mode == sample_cosine)    radiance = sample_cosine_weighted(dg, view_ray);
				else if (sampling_mode == sample_brdf)      radiance = sample_brdfs(dg, view_ray);
#endif
			}
#else
			// todo: compute direct lighting contribution
#endif
		}
#ifndef RTGI_SKIP_SKY
		else
			if (rc->scene.sky)
				radiance = rc->scene.sky->Le(view_ray);
#endif
		result.push_back({radiance,vec2(0)});
	}
	return result;
}

vec3 direct_light::sample_uniformly(const diff_geom &hit, const ray &view_ray) {
	// set up a ray in the hemisphere that is uniformly distributed
	vec2 xi = rc->rng.uniform_float2();
#ifdef RTGI_SKIP_DIRECT_ILLUM_IMPL
	// todo: implement uniform hemisphere sampling
	return vec3(0);
#else
	float z = xi.x;
	float phi = 2*pi*xi.y;
	// z is cos(theta), sin(theta) = sqrt(1-cos(theta)^2)
	float sin_theta = sqrtf(1.0f - z*z);
	vec3 sampled_dir = vec3(sin_theta * cosf(phi),
							sin_theta * sinf(phi),
							z);
	
	vec3 w_i = align(sampled_dir, hit.ng);
	ray sample_ray(hit.x, w_i);

	// find intersection and store brightness if it is a light
	vec3 brightness(0);
	triangle_intersection closest = rc->scene.rt->closest_hit(sample_ray);
	if (closest.valid()) {
		diff_geom dg(closest, rc->scene);
		brightness = dg.mat->emissive;
	}
#ifndef RTGI_SKIP_SKY
	else if (rc->scene.sky)
		brightness = rc->scene.sky->Le(sample_ray);
#endif
	// evaluate reflectance
	return 2*pi * brightness * hit.mat->brdf->f(hit, -view_ray.d, sample_ray.d) * cdot(sample_ray.d, hit.ns);
#endif
}

#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
vec3 direct_light::sample_cosine_weighted(const diff_geom &hit, const ray &view_ray) {
	vec2 xi = rc->rng.uniform_float2();
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING_IMPL
	// todo: implement importance sampling on the cosine-term
	vec3 sampled_dir = cosine_sample_hemisphere(xi);
	vec3 w_i = align(sampled_dir, hit.ng);
	ray sample_ray(hit.x, w_i);

	// find intersection and store brightness if it is a light
	vec3 brightness(0);
	triangle_intersection closest = rc->scene.rt->closest_hit(sample_ray);
	if (closest.valid()) {
		diff_geom dg(closest, rc->scene);
		brightness = dg.mat->emissive;
	}
#ifndef RTGI_SKIP_SKY
	else if (rc->scene.sky)
		brightness = rc->scene.sky->Le(sample_ray);
#endif

	// evaluate reflectance
	return brightness * hit.mat->brdf->f(hit, -view_ray.d, sample_ray.d) * pi;
#else
	return vec3(0);
#endif
}
#endif

vec3 direct_light::sample_lights(const diff_geom &hit, const ray &view_ray) {
#ifdef RTGI_SKIP_DIRECT_ILLUM_IMPL
	// todo: implement uniform sampling on the first few of the scene's lights' surfaces
	return vec3(0);
#elif defined(RTGI_DIRECT_ILLUM_IMPL_SIMPLE)
	const size_t N_max = 2;
	int lighs_processed = 0;
	vec3 accum(0);
	for (int i = 0; i < rc->scene.lights.size(); ++i) {
		const trianglelight *tl = dynamic_cast<trianglelight*>(rc->scene.lights[i]);
		vec2 xi = rc->rng.uniform_float2();
		float sqrt_xi1 = sqrt(xi.x);
		float beta = 1.0f - sqrt_xi1;
		float gamma = xi.y * sqrt_xi1;
		float alpha = 1.0f - beta - gamma;
		const vertex &a = rc->scene.vertices[tl->a];
		const vertex &b = rc->scene.vertices[tl->b];
		const vertex &c = rc->scene.vertices[tl->c];
		vec3 target = alpha*a.pos  + beta*b.pos  + gamma*c.pos;
		vec3 normal = alpha*a.norm + beta*b.norm + gamma*c.norm;
	
		vec3 w_i = target - hit.x;
		float tmax = length(w_i);
		w_i /= tmax;
		ray r(hit.x, w_i);
		r.length_exclusive(tmax);

		if (!rc->scene.rt->any_hit(r)) {
			auto mat = rc->scene.materials[tl->material_id];
			accum += mat.emissive * hit.mat->brdf->f(hit, -view_ray.d, r.d) * cdot(hit.ns, r.d) * cdot(normal, -r.d) / (tmax*tmax);
		}

		if (++lighs_processed == N_max)
			break;
	}
	return accum / (float)lighs_processed;
#elif defined(RTGI_SKIP_IMPORTANCE_SAMPLING_IMPL)
	// todo: implement importance sampling on the light sources
	//       use rc->scene.light_distribution and, once you have found light so sample, trianglelight::sample_Li
	//       don't forget to divide by the respective PDF values
	return vec3(0);
#else
	auto [l_id, l_pdf] = rc->scene.light_distribution->sample_index(rc->rng.uniform_float());
	light *l = rc->scene.lights[l_id];
	auto [shadow_ray,l_col,pdf] = l->sample_Li(hit, rc->rng.uniform_float2());
	if (l_col != vec3(0))
		if (!rc->scene.rt->any_hit(shadow_ray))
			return l_col * hit.mat->brdf->f(hit, -view_ray.d, shadow_ray.d) * cdot(shadow_ray.d, hit.ns) / (pdf * l_pdf);
	return vec3(0);
#endif
}

#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
vec3 direct_light::sample_brdfs(const diff_geom &hit, const ray &view_ray) {
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING_IMPL
	auto [w_i, f, pdf] = hit.mat->brdf->sample(hit, -view_ray.d, rc->rng.uniform_float2());
	ray light_ray(hit.x, w_i);
	if (auto is = rc->scene.rt->closest_hit(light_ray); is.valid())
		if (diff_geom hit_geom(is, rc->scene); hit_geom.mat->emissive != vec3(0))
			return f * hit_geom.mat->emissive * cdot(hit.ns, w_i) / pdf;
	return vec3(0);
#else
	// todo: implement importance sampling of the BRDF-term
	//       use hit.mat->brdf->sample
	//       follow the code there and try to match it with what was prested in the lecture
	return vec3(0);
#endif
}
#endif

bool direct_light::interprete(const std::string &command, std::istringstream &in) {
	string value;
	if (command == "is") {
		in >> value;
		if (value == "uniform") sampling_mode = sample_uniform;
		else if (value == "cosine") sampling_mode = sample_cosine;
		else if (value == "light") sampling_mode = sample_light;
		else if (value == "brdf") sampling_mode = sample_brdf;
		else cerr << "unknown sampling mode in " << __func__ << ": " << value << endl;
		return true;
	}
	return false;
}
#endif


#ifndef RTGI_SKIP_DIRECT_ILLUM
#ifndef RTGI_SKIP_DIRECT_MIS
// separate version to not include the rejection part in all methods
// this should be improved upon
gi_algorithm::sample_result direct_light_mis::sample_pixel(uint32_t x, uint32_t y, uint32_t samples) {
#ifndef RTGI_SKIP_DIRECT_MIS_IMPL
	sample_result result;
	for (int sample = 0; sample < samples; ++sample) {
		vec3 radiance(0);
		ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
		triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
		if (closest.valid()) {
			diff_geom dg(closest, rc->scene);
			flip_normals_to_ray(dg, view_ray);

			if (dg.mat->emissive != vec3(0)) {
				radiance = dg.mat->emissive;
			}
			else {
				brdf *brdf = dg.mat->brdf;
					
				float pdf_light = 0,
					  pdf_brdf = 0;
				if (sample < samples/2-1) {
					auto [l_id, l_pdf] = rc->scene.light_distribution->sample_index(rc->rng.uniform_float());
					light *l = rc->scene.lights[l_id];
					auto [shadow_ray,l_col,pdf] = l->sample_Li(dg, rc->rng.uniform_float2());
					pdf_light = l_pdf*pdf;
					pdf_brdf  = brdf->pdf(dg, -view_ray.d, shadow_ray.d);
					if (l_col != vec3(0))
						if (auto is = rc->scene.rt->closest_hit(shadow_ray); !is.valid() || is.t > shadow_ray.t_max)
							radiance = l_col * brdf->f(dg, -view_ray.d, shadow_ray.d) * cdot(shadow_ray.d, dg.ns);
				}
				else {
					auto [w_i, f, pdf] = brdf->sample(dg, -view_ray.d, rc->rng.uniform_float2());
					ray light_ray(dg.x, w_i);
					pdf_brdf  = pdf;
					if (f != vec3(0))
						if (auto is = rc->scene.rt->closest_hit(light_ray); is.valid())
							if (diff_geom hit_geom(is, rc->scene); hit_geom.mat->emissive != vec3(0)) {
								trianglelight tl(rc->scene, is.ref);
								pdf_light = luma(tl.power()) / rc->scene.light_distribution->integral();
								pdf_light *= tl.pdf(light_ray, hit_geom);
								radiance = f * hit_geom.mat->emissive * cdot(dg.ns, w_i);
							}
				}
				// make sure to really be on the safest side possible ;)
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

#ifndef RTGI_SKIP_SKY
		else
			if (rc->scene.sky)
				radiance = rc->scene.sky->Le(view_ray);
#endif
		result.push_back({radiance,vec2(0)});
	}
	return result;
#else
	sample_result result;
	result.push_back({vec3(0),vec2(0)});
	return result;
#endif
}

bool direct_light_mis::interprete(const std::string &command, std::istringstream &in) {
	// nothing to do but prevent call to base
	return false;
}
#endif
#endif

#ifndef RTGI_SKIP_WF
namespace wf {
	direct_light::direct_light() {
		auto *init_fb = rc->platform->step<initialize_framebuffer>();
		auto *download_fb = rc->platform->step<download_framebuffer>();
		frame_preparation_steps.push_back(init_fb);
		frame_finalization_steps.push_back(download_fb);
		
		camrays = rc->platform->allocate_raydata();
		shadowrays = rc->platform->allocate_raydata();
		pdf = rc->platform->allocate_float_per_sample();
		
		init_fb->use(camrays);
		download_fb->use(camrays);
		
		regenerate_steps();
	}
	void direct_light::regenerate_steps() {
		sampling_steps.clear();
		
		auto *sample_cam   = rc->platform->step<sample_camera_rays>("primary hits");
		auto *find_hit     = rc->platform->step<find_closest_hits>();
		auto *find_light   = rc->platform->step<find_closest_hits>("secondary hits");
		auto *integrate    = rc->platform->step<integrate_light_sample>();
		step *sample_light = nullptr;
		if (sampling_mode == ::direct_light::sample_uniform) {
			auto *sample = rc->platform->step<sample_uniform_dir>();
			sample->use(camrays, shadowrays, pdf);
			sample_light = sample;
		}
		else if (sampling_mode == ::direct_light::sample_cosine) {
			auto *sample = rc->platform->step<sample_cos_weighted_dir>();
			sample->use(camrays, shadowrays, pdf);
			sample_light = sample;
		}
		else throw runtime_error("unsupported importance sampling method for wf/direct");

		sample_cam->use(camrays);
		find_hit->use(camrays);
		find_light->use(shadowrays);
		integrate->use(camrays, shadowrays, pdf);
		
		sampling_steps.push_back(sample_cam);
		sampling_steps.push_back(find_hit);
		sampling_steps.push_back(sample_light);
		sampling_steps.push_back(find_light);
		sampling_steps.push_back(integrate);
	}
	bool direct_light::interprete(const std::string &command, std::istringstream &in) {
		string value;
		if (command == "is") {
			in >> value;
			if (value == "uniform") sampling_mode = ::direct_light::sample_uniform;
			else if (value == "cosine") sampling_mode = ::direct_light::sample_cosine;
			else if (value == "light") sampling_mode = ::direct_light::sample_light;
			else if (value == "brdf") sampling_mode = ::direct_light::sample_brdf;
			else cerr << "unknown sampling mode in " << __func__ << ": " << value << endl;
			regenerate_steps();
			return true;
		}
		return false;
	}
}
#endif

