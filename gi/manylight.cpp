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

//TODO: only for testing: will be replaced by using and sampling the scene lights
pointlight origin_pl = pointlight(vec3(0,-4,0), vec3(35, 30, 26));
vec3 random_dir();

void manylight_algorithm::prepare_frame() {
	/* Generate VPLs */

	/*
		Note to steps 1. - 5.:
		The numbering is set accordingly to the manylight State of The Art Report (STAR):
		https://cgg.mff.cuni.cz/~jaroslav/papers/2013-mlstar/eg2013star_manylights.pdf

		This initialization algorithm is described in chapter 4.1 Random Walk VPL Distribution
	*/

	// 1. initialize j:=0
	int j = 0;
	bool path_terminated = false;

	// setup russian roulette
	//TODO: "activate" RR: currently 'start' is set to the 'max' length => RR only ends after reaching the max. value
	russian_roulette rr(path_length, path_length);

	// begin writing paths.obj
	objdraw::obj_writer obj_writer("paths.obj");
	vector<vec3> obj_vertices;

	for (int i = 0; i < paths; ++i) {
		objdraw::path obj_path(origin_pl.pos);
		vec3 pos = origin_pl.pos;
		vec3 dir = random_dir();

		vec3 throughput(1.0f/paths);

		int j = 0;
		while (!path_terminated) {
			ray ray(pos, dir);

			// 2. sample the next path vertex
			triangle_intersection closest = rc->scene.rt->closest_hit(ray);
			if (!closest.valid()) {
				cout << "NO valid hit! sample: " << i << ", " << "iteration j = " << j << endl;
				break;
			}
			diff_geom hit(closest, rc->scene);

			auto [bounced, pdf] = sample_brdf_distributed_direction(hit, ray);

			if (j == 0) {
				float D = 1.0f;
				throughput *= origin_pl.col * D;
				// D_x(y) is here 1.0f because source is a volumetric point
				// What is p(v_0)? -> "probability that this light source is selected (at this point v_0)"
				// p(w_0) = 1.0f because source is pointlight (in space)
			}
			else {
				vpl previous = vpls.back();
				float D = cdot(dir, previous.geometry.ns);
				vec3 prev_f = previous.geometry.mat->brdf->f(previous.geometry, dir, -previous.w_in);
				float prev_pdf = previous.geometry.mat->brdf->pdf(previous.geometry, dir, -previous.w_in);

				//TODO: divide by q when using russian roulette (q: probability for russian roulette, see paper)
				throughput *= prev_f * D / prev_pdf;
			}

			// 3. create a VPL
			pos = bounced.o;
			vpl v(pos, throughput, hit, dir);
			vpls.push_back(v);

			// set dir for the next iteration
			dir = bounced.d;

			// preparation for obj file
			obj_path.push_vertex(pos);
			obj_vertices.push_back(pos);

			// 4. terminate path
			path_terminated = rr.shot();

			// 5. increment j and go to step 2
			j++;
		}
		obj_writer.write_path(obj_path);

		rr.reset();
		path_terminated = false;
		j = 0;
	}

	// draw path vertices in paths.obj as icospheres
	obj_writer.write_icosphere(objdraw::icosphere(origin_pl.pos));
	for (auto v : obj_vertices) {
		objdraw::icosphere sphere(v, 0.5f);
		obj_writer.write_icosphere(sphere);
	}
}

vec3 manylight_algorithm::sample_pixel(uint32_t x, uint32_t y) {
	/* Render with VPLs */
	vec3 radiance(0);
	int n = vpls.size();

	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (!closest.valid())
		return vec3(0);

	diff_geom dg(closest, rc->scene);

	// if it is a light, add the light's contribution
	if (dg.mat->emissive != vec3(0)) {
		return dg.mat->emissive;
	}

	{
		// direct illumination
		vec3 dir = normalize(dg.x - origin_pl.pos);
		triangle_intersection vpl_closest = rc->scene.rt->closest_hit(ray(origin_pl.pos, dir));
		diff_geom vpl_dg(vpl_closest, rc->scene);
		if (length(vpl_dg.x - dg.x) <= 0.1f) {
			vec3 f = dg.mat->brdf->f(dg, -view_ray.d, -dir); // BRDF at x; BRDF at origin_pl is 1.0f because in space
			float G = cdot(dg.ns, -dir); // D at origin_pl is 1.0f because in space
			// div. by ||o_pl -> x||² needed when using pointlight?
			//G *= 1.0f / (length(dg.x - origin_pl.pos)*length(dg.x - origin_pl.pos));
			radiance += f*G;
		}
	}

	// indirect illumination by using VPLs
	int i = 0;
	for (auto v : vpls) {
		vec3 dir = normalize(dg.x - v.pos);
		triangle_intersection vpl_closest = rc->scene.rt->closest_hit(ray(v.pos, dir));
		diff_geom vpl_dg(vpl_closest, rc->scene);
		float len = length(vpl_dg.x - dg.x);
		if (len > 0.1f) {
			continue;
		}

		// radiance calculation for hitpoint x (dg.x)
		float D_v_to_x = cdot(v.geometry.ns, dir);
		float D_x_to_v = cdot(dg.x, -dir);
		float G = D_v_to_x * D_x_to_v / (length(v.pos - dg.x)*length(v.pos - dg.x));
		float pdf = dg.mat->brdf->pdf(v.geometry, dir, -v.w_in);
		//vec3 throughput = D_v_to_x*v.col*dg.mat->brdf->f(v.geometry, dir, -v.in)*(1.0f/pdf);
		
		vec3 throughput = G*v.col*dg.mat->brdf->f(v.geometry, dir, -v.w_in);

		// radiance calculation for camera / eye
		//float D_x_to_cam = cdot(dg.ns, -view_ray.d); // only relevant for specular lighting?
		pdf = dg.mat->brdf->pdf(dg, -view_ray.d, -dir);
		radiance += throughput*dg.mat->brdf->f(dg, -view_ray.d, -dir);
	}

	return radiance;
}

/*** Util ***/
bool russian_roulette::shot() {
	if (cold_count < start) {
		cold_count++;
		return false;
	}
	if (hot_count >= max_hot) {
		return true;
	}

	float rnd = rc->rng.uniform_float();
	float p = 1.0f/(max_hot-hot_count);
	bool shot = p > rnd;

	hot_count++;
	return shot;
}

void russian_roulette::reset() {
	cold_count = 1;
	hot_count = 0;
}

vec3 random_dir() {
	float x = rc->rng.uniform_float() - 0.5f;
	float y = rc->rng.uniform_float() - 0.5f;
	float z = rc->rng.uniform_float() - 0.5f;
	return normalize(vec3(x,y,z));
}
