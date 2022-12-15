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

void manylight_algorithm::prepare_frame() {
	/* Generate VPLs */

	/*
		Note to steps 1. - 5.:
		The numbering is set accordingly to the manylight State of The Art Report (STAR):
		https://cgg.mff.cuni.cz/~jaroslav/papers/2013-mlstar/eg2013star_manylights.pdf

		This initialization algorithm is described in chapter 4.1 Random Walk VPL Distribution.
		The index j here is used accordingly to the paper.
	*/

	for(int i = 0; i < paths; i++) {
		// Calculate v_0:
		auto [l_id, pdf_l] = rc->scene.light_distribution->sample_index(rc->rng.uniform_float());
        light *l = rc->scene.lights[l_id];
		trianglelight* tl = dynamic_cast<trianglelight*>(l);
		
		/* Start Sample triangle */
		const vertex &a = rc->scene.vertices[tl->a];
		const vertex &b = rc->scene.vertices[tl->b];
		const vertex &c = rc->scene.vertices[tl->c];
		vec2 bc     = uniform_sample_triangle(rc->rng.uniform_float2());
		vec3 target = (1.0f-bc.x-bc.y)*a.pos + bc.x*b.pos + bc.y*c.pos;
    	vec3 n      = (1.0f-bc.x-bc.y)*a.norm + bc.x*b.norm + bc.y*c.norm;
		float area = 0.5f * length(cross(b.pos-a.pos,c.pos-a.pos));
		//TODO: is this pdf correct? / understandable?
		float pdf_tri_sample = 1.f/area;
    	vec3 col = rc->scene.materials[tl->material_id].emissive / (pdf_tri_sample*pdf_l);

		triangle_intersection is;
		is.beta = bc.x;
		is.gamma = bc.y;
		is.ref = tl->tri_id;
		//TODO: I had to modify trianglelight to store its id...
		// What alternatives do I have to create a diff_geom (without simulating a closest_hit or looping through scene.triangles)?

		//is.t = 0.01f; //TODO: is it required to set a t value?
		diff_geom v0_geometry(is, rc->scene);

		// Sample w_0:
		vec3 w_tan = uniform_sample_hemisphere(rc->rng.uniform_float2());
		float pdf_w_0 = uniform_hemisphere_pdf();
		vec3 w_0 = align(w_tan, n);
		/* End Sample triangle */

		vpl v_0(col, v0_geometry);
		vpls.push_back(v_0);

		ray ray(v_0.pos, w_0);
		float q = 1.0f;

		int rr_start = 2; // start RR after this many unrestricted bounces

		// 1. initialize j:=0 and 5. increment j (j:=j+1)
		for (int j = 0; j < path_length; ++j) {
			// get the current (newly created) VPL. This is v_j
			vpl v_j = vpls.back();

			// 2. sample the next path vertex
			triangle_intersection closest = rc->scene.rt->closest_hit(ray);
			if (!closest.valid())
				break;

			diff_geom hit(closest, rc->scene);
			auto [bounced, pdf] = sample_brdf_distributed_direction(hit, ray);

			// 3. create a VPL
			/* Start calculate col for next VPL */
			vec3 v_col;
			if (j == 0) {
				// Calculating throughput for v_1
				float D = cdot(v_0.geometry.ns, ray.d);
				v_col = v_j.col * D / pdf_w_0;
			}
			else {
				// Calculating throughput for v_j+1
				float D = cdot(ray.d, v_j.geometry.ns);
				vec3 f = v_j.geometry.mat->brdf->f(v_j.geometry, ray.d, -v_j.w_in);
				float pdf = v_j.geometry.mat->brdf->pdf(v_j.geometry, ray.d, -v_j.w_in);
				v_col = v_j.col * f * D / (pdf*q);
			}
			/* End calculate col for next VPL */

			// Create and store the next VPL v_j+1
			vpls.push_back(vpl(v_col, hit, ray.d, &v_j));

			// 4. terminate path
			// apply RR
			if (j > rr_start) {
				float xi = uniform_float();
				//TODO: Is it correct to use luma and the albedo here?
				// set q for the next iteration
				q = luma(hit.mat->albedo);
				if (xi >= q)
					break;
			}
			else if (luma(hit.mat->albedo) == 0)
				break;

			// set the ray for the next iteration (points towards the next VPL)
			ray = bounced;
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

    diff_geom dg(closest, rc->scene);
    //flip_normals_to_ray(dg, view_ray); //TODO: is this required?

    // if it is a light, add the light's contribution
    if (dg.mat->emissive != vec3(0)) {
        return dg.mat->emissive;
    }

    for(int i = 0; i < vpls.size(); i++) {
		// direct and indirect illumination
		vpl v = vpls[i];
		auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(dg, rc->rng.uniform_float2());

		if (!rc->scene.rt->any_hit(shadow_ray)) {
			vec3 f_x = dg.mat->brdf->f(dg, -view_ray.d, shadow_ray.d);
			float D_x_v = cdot(dg.ns, shadow_ray.d);
			float D_v_x = cdot(v.geometry.ns, -shadow_ray.d);
			float G = D_x_v*D_v_x/(shadow_ray.t_max*shadow_ray.t_max); // attetntion: should ray epsilon be included in the t² calculation?
	        G = G > 0.1f ? 0.1f : G;

			// for direct illumination:
			vec3 f_v = vec3(1);

			// for indirect illumination:
			if (v.vpl_index > 0)
				f_v = dg.mat->brdf->f(v.geometry, -shadow_ray.d, -v.w_in);

			vec3 col = v.col;
			radiance += f_x*G*col*f_v;
		}
    }
	//TODO: hier durch Anzahl paths teilen richtig? (vgl. mit Paper)
	radiance /= paths;

    return radiance;
}