#include "manylight.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/intersect.h"
#include "libgi/util.h"
#include "libgi/color.h"
#include "libgi/sampling.h"

#include "libgi/timer.h"

#include "libgi/global-context.h"

#include "gi/objdraw.h"

using namespace glm;
using namespace std;

static const bool debug_print = false;

tuple<diff_geom, float, vec3, float> sample_trianglelight(const scene& scene);

void manylight_algorithm::prepare_frame() {
	/* Generate VPLs */

	/*
		Note to steps 1. - 5.:
		The numbering is set accordingly to the manylight State of The Art Report (STAR):
		https://cgg.mff.cuni.cz/~jaroslav/papers/2013-mlstar/eg2013star_manylights.pdf

		This initialization algorithm is described in chapter 4.1 Random Walk VPL Distribution.
		The index j here is used accordingly to the paper.
	*/

	//TODO: can I make this declaration/initialization conditional by debug_print?
	vector<objdraw::path> obj_paths;

	for (int i = 0; i < paths; i++) {
		// Calculate v_0:
		//TODO: should v_0 be "transformed" into a pointlight or should it stay in its original form (e. g. trianglelight)
		//TODO: currently it is only possible to sample from trianglelights
		auto [geometry_v_0, pdf_v_0, w_0, pdf_w_0] = sample_trianglelight(rc->scene);
		
		vec3 Le_v_0(geometry_v_0.mat->emissive);
		vpl v_0(Le_v_0, geometry_v_0);
		sampled_lights.push_back(sample_context(v_0, pdf_v_0));
		objdraw::path obj_path(v_0.pos); //TODO: can I make this declaration/initialization conditional by debug_print?

		ray to_next_vpl(v_0.pos, w_0);
		vec3 throughput(1);

		int rr_start = 4; // start RR after this many unrestricted bounces

		{
			// Setup the throughput for VPL v_1
			float D = cdot(v_0.geometry.ns, to_next_vpl.d); //D_v_0(v_1)
			throughput *= D / (pdf_v_0*pdf_w_0);
		}

		// 1. initialize j:=0 and 5. increment j (j:=j+1)
		for (int j = 1; j <= path_length; ++j) {
			// 2. sample the next path vertex
			triangle_intersection closest = rc->scene.rt->closest_hit(to_next_vpl);
			if (!closest.valid())
				break;
			diff_geom hit(closest, rc->scene);

			// 3. create a VPL (v_j)
			vpl v_j(Le_v_0 * throughput, hit, to_next_vpl.d);
			vpls.push_back(v_j);
			if (debug_print)
				obj_path.push_vertex(v_j.pos);

			// 4. Terminate path (apply RR)
			//TODO: throughput often bigger than 1 because of the pdfs
			if (j >= rr_start) {
				float xi = uniform_float();
				float q = luma(throughput);
				//TODO: is this q_j or q_j+1?
				// -> equals: float q = luma(v_j.col/Le_v_0);
				//TODO: should throughput be divided by paths?
				
				if (xi >= q)
					break;

				throughput *= 1.0f/q;
			}
			else if (luma(throughput) == 0)
				break;

			// Sample ray to next VPL
			auto [w_i, f, pdf] = hit.mat->brdf->sample(v_j.geometry, -v_j.w_in, rc->rng.uniform_float2()); //f(v_j-1->v_j->v_j+1)
			float pdf_f = hit.mat->brdf->pdf(v_j.geometry, w_i, -v_j.w_in); //p(w_j)
			to_next_vpl = ray(v_j.pos, w_i);

			// Setup the throughput for the next VPL
			float D = cdot(to_next_vpl.d, v_j.geometry.ns); //D_v_j(v_j+1)
			//TODO: Which pdf is correct? 'pdf' from sample or 'pdf_f'?
			throughput *= D*f/pdf_f; //throughput for v_j+1
		}

		obj_paths.push_back(obj_path);
	}

	if (debug_print) {
		// begin writing paths.obj
		objdraw::obj_writer obj_writer("paths.obj");

		for (auto& p : obj_paths)
			obj_writer.write_path(p);

		// draw path vertices in paths.obj as icospheres
		for (auto c : sampled_lights) {
			objdraw::icosphere sphere(c.v.pos, 0.15f);
			obj_writer.write_icosphere(sphere);
		}
		for (auto v : vpls) {
			objdraw::icosphere sphere(v.pos, 0.25f);
			obj_writer.write_icosphere(sphere);
		}
	}
}

vec3 manylight_algorithm::sample_pixel(uint32_t x, uint32_t y) {
	/* Render with VPLs */
	vec3 radiance(0);

	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (!closest.valid())
		return vec3(0);

	diff_geom hit(closest, rc->scene);
	flip_normals_to_ray(hit, view_ray);

	// if it is a light, add the light's contribution
	if (hit.mat->emissive != vec3(0)) {
		return hit.mat->emissive;
	}

	// direct illumination
	{
		for (int i = 0; i < sampled_lights.size(); ++i) {
			sample_context& sampled_light = sampled_lights[i];
			float pdf = sampled_light.pdf;
			vpl& vpl = sampled_light.v;
			auto [shadow_ray, col_delete, pdf_delete] = vpl.sample_Li(hit, rc->rng.uniform_float2());
			float t = length(vpl.pos - hit.x);
			float D_x_v = cdot(shadow_ray.d, hit.ns);
			float D_v_x = cdot(vpl.geometry.ns, -shadow_ray.d);
			float factor_sr = t*t/D_v_x;
			float G = D_x_v*D_v_x/(t*t);
			G = G > 0.1f ? 0.1f : G;
			if (vpl.col != vec3(0))
				if (!rc->scene.rt->any_hit(shadow_ray))
					radiance += vpl.col * hit.mat->brdf->f(hit, -view_ray.d, shadow_ray.d) * G / pdf;
					//TODO: What approach do we take?
					//radiance += vpl.col * hit.mat->brdf->f(hit, -view_ray.d, shadow_ray.d) * D_x_v / (pdf*factor_sr);
		}
	}

	// indirect illumination by using VPLs
	for (int i = 0; i < vpls.size(); ++i) {
		vpl v = vpls[i];
		auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(hit, rc->rng.uniform_float2());
		float t = length(v.pos - hit.x);

		if (!rc->scene.rt->any_hit(shadow_ray)) {
			vec3 f_x = hit.mat->brdf->f(hit, -view_ray.d, shadow_ray.d); // BRDF at x (hit)
			vec3 f_v = v.geometry.mat->brdf->f(v.geometry, -shadow_ray.d, -v.w_in); // BRDF at v

			float D_x_v = cdot(hit.ns, shadow_ray.d);
			float D_v_x = cdot(v.geometry.ns, -shadow_ray.d);
			float G = D_x_v*D_v_x/(t*t);
			//TODO: G cap solves the issue with the bright spots but adds bias
			//-> see chapter 5: bias compensation (final gathering, ...)
			G = G > 0.1f ? 0.1f : G;

			radiance += f_x*G*v.col*f_v;
		}
	}
	//TODO: hier durch Anzahl paths teilen verständlich? (vgl. mit Paper)
	//      Oder: schon in prepare_frame bei jedem VPL einzeln, wie im Paper beschrieben
	radiance /= paths;

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
	vec2 bc     = uniform_sample_triangle(rc->rng.uniform_float2());
	float area  = 0.5f * length(cross(b.pos-a.pos,c.pos-a.pos));
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