#include "manylight.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"
#include "libgi/sampling.h"

#include "libgi/timer.h"

#include "libgi/global-context.h"

#include "libgi/objdraw.h"

using namespace glm;
using namespace std;

static const bool export_debug_obj = false;

tuple<diff_geom, float, vec3, float> sample_trianglelight(const scene& scene);

void manylight_algorithm::prepare_frame() {
	/*
		Note to steps 1. - 5.:
		The numbering is set accordingly to the manylight State of The Art Report (STAR):
		https://cgg.mff.cuni.cz/~jaroslav/papers/2013-mlstar/eg2013star_manylights.pdf

		This initialization algorithm is described in chapter 4.1 Random Walk VPL Distribution.
		The index j here is used accordingly to the paper.
	*/

	vector<objdraw::path> obj_paths; // list of all sampled paths
	vector<vpl> obj_v_0_samples;  // list of all sampled v_0 lights

	for (int i = 0; i < paths; i++) {
		// Calculate v_0:
		//TODO: currently it is only possible to sample from trianglelights
		auto [geometry_v_0, pdf_v_0, w_0, pdf_w_0] = sample_trianglelight(rc->scene);
		
		vec3 Le_v_0(geometry_v_0.mat->emissive);
		vpl v_0(Le_v_0 * (1.0f/paths), geometry_v_0);
		obj_v_0_samples.push_back(v_0);
		objdraw::path obj_path(v_0.pos);

		ray to_next_vpl(v_0.pos, w_0);
		vec3 throughput(1);

		int rr_start = 4; // start RR after this many unrestricted bounces

		// Setup the throughput for VPL v_1
		float D_v_0 = cdot(v_0.geometry.ns, to_next_vpl.d); //D_v_0(v_1)
		throughput *= D_v_0 / (pdf_v_0*pdf_w_0);

		// 1. initialize j:=0 and 5. increment j (j:=j+1)
		for (int j = 1; j <= path_length; ++j) {
			// 2. sample the next path vertex
			triangle_intersection closest = rc->scene.rt->closest_hit(to_next_vpl);
			if (!closest.valid())
				break;
			diff_geom hit(closest, rc->scene);

			// 3. create a VPL (v_j)
			vpl v_j(Le_v_0 * throughput * (1.0f), hit, to_next_vpl.d);
			vpls.push_back(v_j);
			if (export_debug_obj)
				obj_path.push_vertex(v_j.pos);

			// 4. Terminate path (apply RR)
			if (j >= rr_start) {
				float xi = uniform_float();
				float q = luma(throughput);
				//TODO: is this q_j or q_j+1?
				// -> equals: float q = luma(v_j.col/Le_v_0);
				
				if (xi >= q)
					break;

				throughput *= 1.0f/q;
			}
			else if (luma(throughput) == 0)
				break;

			// Sample ray to next VPL
			auto [w_o, f, pdf] = hit.mat->brdf->sample(v_j.geometry, -v_j.w_in, rc->rng.uniform_float2()); //f(v_j-1->v_j->v_j+1)

			// Note: 'pdf_f' does not equal 'pdf' (returned from 'sample')
			float pdf_f = hit.mat->brdf->pdf(v_j.geometry, w_o, -v_j.w_in); //p(w_j)
			to_next_vpl = ray(v_j.pos, w_o);

			// Setup the throughput for the next VPL
			float D = cdot(to_next_vpl.d, v_j.geometry.ns); //D_v_j(v_j+1)
			throughput *= D*f/pdf_f; //throughput for v_j+1
		}

		obj_paths.push_back(obj_path);
	}

	avg_path_length = vpls.size() * (1.0f/(paths));

	if (export_debug_obj) {
		// begin writing paths.obj
		objdraw::obj_writer obj_writer("paths.obj");

		for (auto& p : obj_paths)
			obj_writer.write_path(p);

		// draw path vertices in paths.obj as icospheres
		for (auto v_0 : obj_v_0_samples) {
			objdraw::icosphere sphere(v_0.pos, 0.15f);
			obj_writer.write_icosphere(sphere);
		}
		for (auto v : vpls) {
			objdraw::icosphere sphere(v.pos, 0.25f);
			obj_writer.write_icosphere(sphere);
		}
	}
}

vec3 manylight_algorithm::sample_pixel(uint32_t x, uint32_t y) {
	vec3 radiance(0);

	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (!closest.valid())
		return vec3(0);

	diff_geom hit(closest, rc->scene);
	flip_normals_to_ray(hit, view_ray);

	// if it is a light, add the light's contribution
	if (hit.mat->emissive != vec3(0))
		return hit.mat->emissive;
	
	// direct illumination
	brdf *brdf = hit.mat->brdf;
	if      (sampling_mode == sample_uniform)   radiance = sample_uniformly(hit, view_ray);
	else if (sampling_mode == sample_light)     radiance = sample_lights(hit, view_ray);
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	else if (sampling_mode == sample_cosine)    radiance = sample_cosine_weighted(hit, view_ray);
	else if (sampling_mode == sample_brdf)      radiance = sample_brdfs(hit, view_ray);
#endif

	// indirect illumination by using VPLs
	for (int i = 0; i < avg_path_length; ++i) {
		// sample a random VPL
		//int32_t pos = rc->rng.uniform_float() * vpls.size();
		//vpl v = vpls[pos];

		vpl v = vpls[vpl_index];
		vpl_index = (vpl_index+1) % vpls.size();

		auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(hit, rc->rng.uniform_float2());
		float t = length(v.pos - hit.x);

		if (!rc->scene.rt->any_hit(shadow_ray)) {
			vec3 f_x = hit.mat->brdf->f(hit, -view_ray.d, shadow_ray.d); // BRDF at x (hit)
			vec3 f_v = v.geometry.mat->brdf->f(v.geometry, -shadow_ray.d, -v.w_in); // BRDF at v

			float D_x = cdot(hit.ns, shadow_ray.d); // D_x(v)
			float D_v = cdot(v.geometry.ns, -shadow_ray.d); // D_v(x)
			float G = D_x*D_v/(t*t);
			//TODO: G cap solves the issue with the bright spots but adds bias
			//-> see chapter 5: bias compensation (final gathering, ...)
			G = G > 0.1f ? 0.1f : G;

			radiance += f_x*G*v.col*f_v;
		}
	}

	return radiance;
}

/* Util */
tuple<diff_geom, float, vec3, float> sample_trianglelight(const scene& scene) {
	auto [l_id, pdf_l] = rc->scene.light_distribution->sample_index(rc->rng.uniform_float());
	light *l = rc->scene.lights[l_id];
	trianglelight* tl = dynamic_cast<trianglelight*>(l);

	const vertex &a = rc->scene.vertices[tl->a];
	const vertex &b = rc->scene.vertices[tl->b];
	const vertex &c = rc->scene.vertices[tl->c];
	vec2 bc = uniform_sample_triangle(rc->rng.uniform_float2());
	float area = 0.5f * length(cross(b.pos-a.pos,c.pos-a.pos));
	float pdf_tri_sample = 1.f/area;
	float pdf_v = pdf_l * pdf_tri_sample;

	triangle_intersection is;
	is.beta = bc.x;
	is.gamma = bc.y;
	diff_geom geometry(*tl, is, rc->scene);
	// Note: is.t, is.ref and accordingly geometry.ref are not set (correctly), but are also not required here

	// Sample w:
	vec3 w_tan = cosine_sample_hemisphere(rc->rng.uniform_float2());
	vec3 w = align(w_tan, geometry.ns);
	float pdf_w = cosine_hemisphere_pdf(cdot(w, geometry.ns));

	return {geometry, pdf_v, w, pdf_w};
}