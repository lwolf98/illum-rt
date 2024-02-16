#include "direct.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"
#include "libgi/sampling.h"

#include "libgi/global-context.h"

#include "debug/pixel.h"
#include "debug/obj_writer.h"

#include "config.h"

#ifdef HAVE_GL
#include "driver/preview.h"
#endif

using namespace glm;
using namespace std;

#ifdef WITH_OBJ_DEBUG
def_obj_debug;
#endif

#ifndef RTGI_SKIP_ASS
/*! Find the closest non-perfectly-specular hitpoint in a given direction.
 *
 *  It can happen that sampling the BRDF yields an unreasonable direction
 *  (due to shading vs geometry normals, eg) and in this cases we terminate
 *  the specular path and return (via the last tuple element) that the path
 *  is useless.
 */
std::tuple<triangle_intersection,vec3,bool> find_closest_nonspecular(ray &ray, int max_bounces = 8) {
	auto is = rc->scene.rt->closest_hit(ray);
	vec3 throughput(1);
	bool valid_path = true;
#ifdef WITH_OBJ_DEBUG
	if (debug)
		std::cout << "path" << std::endl;
#endif
	while (is.valid()) {
		if (--max_bounces == 0) { throughput = vec3(0); break; }
		diff_geom hit(is, rc->scene);
		if (dynamic_cast<specular_brdf*>(hit.mat->brdf)) {
#ifdef WITH_OBJ_DEBUG
			if (debug)
				*ow << obj::line(ray.o, hit.x);
#endif
			auto xis = rc->rng.uniform_float2();
			auto [w_i, f, pdf, _] = hit.mat->brdf->sample(hit, -ray.d, xis);
			if (pdf == 0) {
				valid_path = false;
				break;
			}
			// missing: \cos\theta
			throughput *= f / pdf;
			ray = ::ray(hit.x, w_i);
			is = rc->scene.rt->closest_hit(ray);
		}
		else break;
	}
	return {is, throughput, valid_path };
}
#endif

#ifndef RTGI_SKIP_DIRECT_ILLUM
vec3 direct_light::sample_pixel(uint32_t x, uint32_t y) {
#ifdef WITH_OBJ_DEBUG
	start_obj_debug(x, y, "/tmp/debug.obj");
	if (debug)
		*ow << obj::object("path");
#endif

	vec3 radiance(0,0,0);
	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
#ifndef RTGI_SKIP_ASS
	auto [closest,tp,valid] = find_closest_nonspecular(view_ray);
	if (valid)
#else
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
#endif
		if (closest.valid()) {
			diff_geom dg(closest, rc->scene);

			// denoising example
			if (rc->enable_denoising) {
				rc->framebuffer_normal.add(x, y, dg.ns);
				rc->framebuffer_albedo.add(x, y, dg.albedo());
			}

#ifndef RTGI_SKIP_DIRECT_ILLUM_IMPL
			if (dg.mat->emissive != vec3(0)) {
				radiance = dg.mat->emissive;
			}
			else {
				//auto col = dg.mat->albedo_tex ? dg.mat->albedo_tex->sample(dg.tc) : dg.mat->albedo;
				if      (sampling_mode == sample_uniform)   radiance = sample_uniformly(dg, view_ray);
				else if (sampling_mode == sample_light)     radiance = sample_lights(dg, view_ray);
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
				else if (sampling_mode == sample_cosine)    radiance = sample_cosine_weighted(dg, view_ray);
				else if (sampling_mode == sample_brdf)      radiance = sample_brdfs(dg, view_ray);
#endif
			}
#else
			// todo: compute direct lighting contribution.
			// delegate to sample_uniformly or sample_lights to implement the actual sampling,
#endif
		}
#ifndef RTGI_SKIP_SKY
		else
			if (rc->scene.sky)
				radiance = rc->scene.sky->Le(view_ray);
#endif
#ifndef RTGI_SKIP_ASS
	return radiance*tp;
#else
	return radiance;
#endif
}

vec3 direct_light::sample_uniformly(const diff_geom &hit, const ray &view_ray) {
	// set up a ray in the hemisphere that is uniformly distributed
	vec2 xi = rc->rng.uniform_float2();
#ifdef RTGI_SKIP_DIRECT_ILLUM_IMPL
	// todo: Implement uniform hemisphere sampling by computing directions as described in the lecture.
	// Note that we always compute such directions with the z-axis pointing upwards in the hemisphere,
	// but that this does not generally correspond to the actual geometry we place the hemisphere on.
	// To that end, use the \c align function (see util.h) to align the sampled direction with the
	// hit-geometry's orientation.
	// With that direction, compute one sample for the DII by casting a ray and evaluating the integrand
	// with the proper scaling factors (see MC estimator for the DII)
	// Note that the normalization by the number of samples is taken of care outside of this function.
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
#ifndef RTGI_SKIP_ASS
	auto [closest,tp,ok] = find_closest_nonspecular(sample_ray);
	if (!ok) return vec3(0,0,0);
#else
	triangle_intersection closest = rc->scene.rt->closest_hit(sample_ray);
#endif
	if (closest.valid()) {
		diff_geom dg(closest, rc->scene);
		brightness = dg.mat->emissive;
	}
#ifndef RTGI_SKIP_SKY
	else if (rc->scene.sky)
		brightness = rc->scene.sky->Le(sample_ray);
#endif
	// evaluate reflectance
#ifndef RTGI_SKIP_ASS
	return 2*pi * brightness * tp * hit.mat->brdf->f(hit, -view_ray.d, sample_ray.d) * cdot(sample_ray.d, hit.ns);
#else
	return 2*pi * brightness * hit.mat->brdf->f(hit, -view_ray.d, sample_ray.d) * cdot(sample_ray.d, hit.ns);
#endif
#endif
}

#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
vec3 direct_light::sample_cosine_weighted(const diff_geom &hit, const ray &view_ray) {
	vec2 xi = rc->rng.uniform_float2();
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING_IMPL
	vec3 sampled_dir = cosine_sample_hemisphere(xi);
	vec3 w_i = align(sampled_dir, hit.ng);
	ray sample_ray(hit.x, w_i);

	// find intersection and store brightness if it is a light
	vec3 brightness(0);
#ifndef RTGI_SKIP_ASS
	auto [closest,tp,ok] = find_closest_nonspecular(sample_ray);
	if (!ok) return vec3(0,0,0); // NOTE: this introduces bias as we accumulate a black sample!
#else
	triangle_intersection closest = rc->scene.rt->closest_hit(sample_ray);
#endif
	if (closest.valid()) {
		diff_geom dg(closest, rc->scene);
		brightness = dg.mat->emissive;
	}
#ifndef RTGI_SKIP_SKY
	else if (rc->scene.sky)
		brightness = rc->scene.sky->Le(sample_ray);
#endif

	// evaluate reflectance
#ifndef RTGI_SKIP_ASS
	return brightness * tp * hit.mat->brdf->f(hit, -view_ray.d, sample_ray.d) * pi;
#else
	return brightness * hit.mat->brdf->f(hit, -view_ray.d, sample_ray.d) * pi;
#endif
#else
	// todo: implement importance sampling on the cosine-term
	return vec3(0);
#endif
}
#endif

vec3 direct_light::sample_lights(const diff_geom &hit, const ray &view_ray) {
#ifdef RTGI_SKIP_DIRECT_ILLUM_IMPL
	// todo: Implement uniform sampling on the first few of the scene's lights' surfaces To this
	// end, convert the lights to triangle lights, take two random numbers and compute a position on
	// the light according to the information on the assignment sheet. 
	// Note that this direction is already in the correct coordinate frame.
	// Use the thusly sampled direction to evaluate the area formulation of the DII.
	//
	// Note: to safely convert the light use dynamic_cast. This is not an
	// elegant solution, but works for this intermediate implementation as it
	// will be replaced in the next assignment.
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
	//       use rc->scene.light_distribution and, once you have found a light to sample, trianglelight::sample_Li (via light::sample_Li)
	//       return the full value of the DII for this sample and don't forget to divide by the respective PDF values
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
	auto [w_i, f, pdf, _] = hit.mat->brdf->sample(hit, -view_ray.d, rc->rng.uniform_float2());
	ray light_ray(hit.x, w_i);
	if (f != vec3(0)) {
#ifndef RTGI_SKIP_ASS
		if (auto [is,tp,ok] = find_closest_nonspecular(light_ray); ok)
			if (is.valid()) {
				if (diff_geom hit_geom(is, rc->scene); hit_geom.mat->emissive != vec3(0))
					return tp * f * hit_geom.mat->emissive * cdot(hit.ns, w_i) / pdf;
			}
#else
		triangle_intersection is = rc->scene.rt->closest_hit(light_ray);
		if (is.valid()) {
			if (diff_geom hit_geom(is, rc->scene); hit_geom.mat->emissive != vec3(0))
				return f * hit_geom.mat->emissive * cdot(hit.ns, w_i) / pdf;
		}
#endif
#ifndef RTGI_SKIP_SKY
			else if (rc->scene.sky)
#ifndef RTGI_SKIP_ASS
				return tp * f * rc->scene.sky->Le(light_ray) / pdf;
#else
				return f * rc->scene.sky->Le(light_ray) / pdf;
#endif
#endif
	}
	return vec3(0);
#else
	// todo: implement importance sampling of the BRDF-term
	//       use hit.mat->brdf->sample
	//       follow the code there and try to match it with what was presented in the lecture
	return vec3(0);
#endif
}
#endif

void direct_light::finalize_frame() {
	if (rc->enable_denoising) {
		rc->framebuffer_albedo.color.for_each([](unsigned int x, unsigned int y) {
			rc->framebuffer_albedo.color.data[y * rc->framebuffer_albedo.color.w + x] /= rc->sppx;
		});
		rc->albedo_valid = true;
		rc->framebuffer_normal.color.for_each([](unsigned int x, unsigned int y) {
			rc->framebuffer_normal.color.data[y * rc->framebuffer_normal.color.w + x] /= rc->sppx;
		});
		rc->normal_valid = true;
	}
}

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
#ifdef RTGI_SKIP_DIRECT_MIS_IMPL
// separate version to not include the rejection part in all methods
// this should be improved upon
#endif
vec3 direct_light_mis::sample_pixel(uint32_t x, uint32_t y) {
#ifndef RTGI_SKIP_DIRECT_MIS_IMPL
	vec3 radiance(0);
	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
#ifndef RTGI_SKIP_ASS
	auto [closest,tp,valid] = find_closest_nonspecular(view_ray);
	if (valid && closest.valid()) {
#else
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (closest.valid()) {
#endif
		while (true) { // will repeat if MIS heuristic yields 0 (rejection sampling)
			diff_geom dg(closest, rc->scene);
			
			if (dg.mat->emissive != vec3(0)) {
				radiance = dg.mat->emissive;
			}
			else {
				brdf *brdf = dg.mat->brdf;
					
				float pdf_light = 0,
					  pdf_brdf = 0;
				if (current_sample_index < rc->sppx/2-1) {
					auto [l_id, l_pdf] = rc->scene.light_distribution->sample_index(rc->rng.uniform_float());
					light *l = rc->scene.lights[l_id];
					auto [shadow_ray,l_col,pdf] = l->sample_Li(dg, rc->rng.uniform_float2());
					pdf_light = l_pdf*pdf;
					#ifndef BAD_MIS
					pdf_brdf  = brdf->pdf(dg, -view_ray.d, shadow_ray.d);
					#endif
					if (l_col != vec3(0))
						if (auto is = rc->scene.rt->closest_hit(shadow_ray); !is.valid() || is.t > shadow_ray.t_max) // TODO why is this not anyhit?
							radiance = l_col * brdf->f(dg, -view_ray.d, shadow_ray.d) * cdot(shadow_ray.d, dg.ns);
				}
				else {
					auto [w_i, f, pdf, _] = brdf->sample(dg, -view_ray.d, rc->rng.uniform_float2());
					if (pdf == 0 && f == vec3(0)) break; // this will loop forever as balance will become 0 (in case pdf==0 originates from geometry (shading normals))
					ray light_ray(dg.x, w_i);
					pdf_brdf  = pdf;
					if (f != vec3(0))
#ifndef RTGI_SKIP_ASS
						if (auto [is,tp,ok] = find_closest_nonspecular(light_ray); ok && is.valid())
#else
						if (auto is = rc->scene.rt->closest_hit(light_ray); is.valid())
#endif
							if (diff_geom hit_geom(is, rc->scene); hit_geom.mat->emissive != vec3(0)) {
								// Need to document this!!!
								trianglelight tl(rc->scene, is.ref);
								#ifndef BAD_MIS
								pdf_light = luma(tl.power()) / rc->scene.light_distribution->integral();
								pdf_light *= tl.pdf(light_ray, hit_geom);
								#endif
#ifndef RTGI_SKIP_ASS
								radiance = tp * f * hit_geom.mat->emissive * cdot(dg.ns, w_i);
#else
								radiance = f * hit_geom.mat->emissive * cdot(dg.ns, w_i);
#endif
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
				#ifndef BAD_MIS
				float balance = pdf_light + pdf_brdf; // 1920/229
				if (balance != 0.0f)
					radiance /= balance*0.5;
				else 
					continue;
				#else
				radiance /= (pdf_brdf + pdf_light); // only one will be != 0 here
				#endif
			}
			
			if (rc->enable_denoising) {
				rc->framebuffer_normal.add(x, y, dg.ns);
				rc->framebuffer_albedo.add(x, y, dg.albedo());
			}
			break;
		}
	}
#ifndef RTGI_SKIP_SKY
	else
		if (rc->scene.sky)
			radiance = rc->scene.sky->Le(view_ray);
#endif
#ifndef RTGI_SKIP_ASS
	return radiance*tp;
#else
	return radiance;
#endif
#else
	/* todo: implement MIS for light and brdf sampling.
	 * the outline is the same as for the non-mis variant, the sampling part differs.
	 * to get the "global" index of the current sample use current_sample_index (cf algorithm.h and the exercise sheet).
	 * tip: initialize both pdf values to 0 before selecting which sampling scheme to use and fill them in each branch, both.
	 *      then use the results consistently and put in a ton of asserts.
	 * tip: A TON
	 * 	assert(pdf_light >= 0);
	 *  assert(pdf_brdf >= 0);
	 *  assert(radiance.x >= 0);assert(std::isfinite(radiance.x));
	 *  assert(radiance.y >= 0);assert(std::isfinite(radiance.y));
	 *  assert(radiance.z >= 0);assert(std::isfinite(radiance.z));
	 *  assert(std::isfinite(pdf_light));
	 *  assert(std::isfinite(pdf_brdf));
	 *
	 * also, include sky sampling for the background, see direct. you might want to check out the sky "assignment" first.
	 */
	return vec3(0);
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
		// TODO: remove those two?
		frame_preparation_steps.push_back(init_fb);
		frame_finalization_steps.push_back(download_fb);
		
		camrays = rc->platform->allocate_raydata();
		shadowrays = rc->platform->allocate_raydata();
		pdf = rc->platform->allocate_float_per_sample();
		
		regenerate_steps();
	}
	void direct_light::regenerate_steps() {
		frame_preparation_steps.clear();
		frame_finalization_steps.clear();
		
		auto *init_fb = rc->platform->step<initialize_framebuffer>();
		auto *download_fb = rc->platform->step<download_framebuffer>();
		
		frame_preparation_steps.push_back(init_fb);
		frame_finalization_steps.push_back(download_fb);

		init_fb->use(camrays);
		download_fb->use(camrays);

		sampling_steps.clear();
		
		auto *sample_cam   = rc->platform->step<sample_camera_rays>("primary hits");
		auto *find_hit     = rc->platform->step<find_closest_hits>();
		step *find_light   = nullptr;
		step *integrate    = nullptr;
		step *sample_light = nullptr;
		if (sampling_mode == ::direct_light::sample_uniform) {
			auto *sample  = rc->platform->step<sample_uniform_dir>();
			auto *trace   = rc->platform->step<find_closest_hits>("secondary hits");
			auto *contrib = rc->platform->step<integrate_dir_sample>();
			sample_light  = sample;
			find_light    = trace;
			integrate     = contrib;
			sample->use(camrays, shadowrays, pdf);
			trace->use(shadowrays);
			contrib->use(camrays, shadowrays, pdf);
			delete lightcol; lightcol = nullptr;
		}
		else if (sampling_mode == ::direct_light::sample_cosine) {
			auto *sample  = rc->platform->step<sample_cos_weighted_dir>();
			auto *trace   = rc->platform->step<find_closest_hits>("secondary hits");
			auto *contrib = rc->platform->step<integrate_dir_sample>();
			sample_light  = sample;
			find_light    = trace;
			integrate     = contrib;
			sample->use(camrays, shadowrays, pdf);
			trace->use(shadowrays);
			contrib->use(camrays, shadowrays, pdf);
			delete lightcol; lightcol = nullptr;
		}
		else if (sampling_mode == ::direct_light::sample_light) {
			// for this case we also have to compute the light distribution
			auto *l_dist = rc->platform->step<compute_light_distribution>();
			data_reset_steps.push_back(l_dist);
			lightcol = rc->platform->allocate_vec3_per_sample();
		
			auto *sample  = rc->platform->step<sample_light_dir>();
			auto *trace   = rc->platform->step<find_any_hits>("find occluders");
			auto *contrib = rc->platform->step<integrate_light_sample>();
			sample_light  = sample;
			find_light    = trace;
			integrate     = contrib;
			sample->use(camrays, shadowrays, pdf, l_dist, lightcol);
			trace->use(shadowrays);
			contrib->use(camrays, shadowrays, pdf, lightcol);
		}
		else throw runtime_error("unsupported importance sampling method for wf/direct");

		sample_cam->use(camrays);
		find_hit->use(camrays);

		sampling_steps.push_back(sample_cam);
		sampling_steps.push_back(find_hit);
		sampling_steps.push_back(sample_light);
		sampling_steps.push_back(find_light);
		sampling_steps.push_back(integrate);

		// For denoising
		auto *hit_albedo = rc->platform->step<add_hitpoint_albedo_to_framebuffer>();
		auto *hit_normal = rc->platform->step<add_hitpoint_normal_to_framebuffer>();
		hit_albedo->use(camrays);
		hit_normal->use(camrays);
		sampling_steps.push_back(hit_albedo);
		sampling_steps.push_back(hit_normal);
		
#ifdef HAVE_GL
		if (preview_window) {
			auto *copy_prev = rc->platform->step<copy_to_preview>();
			sampling_steps.push_back(copy_prev); // add this last so we have data to copy
			copy_prev->use(camrays);
		}
#endif
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

