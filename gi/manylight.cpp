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

void manylight_algorithm::prepare_frame() {
	/*
		Note to steps 1. - 5.:
		The numbering is set accordingly to the manylight State of The Art Report (STAR):
		https://cgg.mff.cuni.cz/~jaroslav/papers/2013-mlstar/eg2013star_manylights.pdf

		This initialization algorithm is described in chapter 4.1 Random Walk VPL Distribution.
		The index j here is used accordingly to the paper.
	*/

	vector<vector<vpl>> vpl_store(paths);
	vector<objdraw::path> obj_paths(paths); // list of all sampled paths
	vector<vec3> obj_v_0_samples(paths);  // list of all sampled v_0 lights
	int rr_start = 4; // start RR after this many unrestricted bounces

	{
		time_this_block(ml_prepare_vpls);

		#pragma omp parallel for
		for (int i = 0; i < paths; i++) {
			// Calculate v_0:
			auto [l_id, pdf_l] = rc->scene.light_distribution->sample_index(rc->rng.uniform_float());
			light *l = rc->scene.lights[l_id];
			vec2 xis_pos = rc->rng.uniform_float2();
			vec2 xis_dir = rc->rng.uniform_float2();
			auto [to_next_vpl, Le_v_0, normal_v_0, pdf_Le] = l->sample_Le(xis_pos, xis_dir);
			float pdf_v_0 = pdf_l * pdf_Le;
			
			obj_v_0_samples[i] = to_next_vpl.o;
			objdraw::path obj_path(to_next_vpl.o);

			// Setup the throughput for VPL v_1
			float D_v_0 = cdot(normal_v_0, to_next_vpl.d); //D_v_0(v_1)
			vec3 throughput(1);
			throughput *= D_v_0 / pdf_v_0;

			// 1. initialize j and 5. increment j (j:=j+1)
			// j represents the VPL index, e. g. in the loop j=1: v_1 is calculated
			for (int j = 1; j <= path_length; ++j) {
				// 2. sample the next path vertex
				triangle_intersection closest = rc->scene.rt->closest_hit(to_next_vpl);
				if (!closest.valid())
					break;

				diff_geom hit(closest, rc->scene);
				flip_normals_to_ray(hit, to_next_vpl);

				// 3. create a VPL (v_j)
				vpl v_j(Le_v_0 * throughput * (1.f/paths), hit.x, hit.ns, to_next_vpl.d, closest);
				if (v_j.col != v_j.col)
					cout << "NAN! Le: " << Le_v_0 << ", Tp: " << throughput << endl;
				vpl_store[i].push_back(v_j);
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
				diff_geom v_j_geometry = hit;
				auto [w_o, f, pdf] = hit.mat->brdf->sample(v_j_geometry, -v_j.w_in, rc->rng.uniform_float2()); //f(v_j-1->v_j->v_j+1)

				// Note: 'pdf_f' does not equal 'pdf' (returned from 'sample')
				float pdf_f = hit.mat->brdf->pdf(v_j_geometry, w_o, -v_j.w_in); //p(w_j)
				to_next_vpl = ray(v_j.pos, w_o);

				// Setup the throughput for the next VPL
				float D = cdot(to_next_vpl.d, v_j.normal); //D_v_j(v_j+1)
				throughput *= D*f/pdf_f; //throughput for v_j+1
			}

			obj_paths[i] = obj_path;
		}
	}

	{
		time_this_block(ml_prepare_copying);
		for (auto path : vpl_store)
			for (auto v : path)
				vpls.push_back(v);
	}

	if (export_debug_obj) {
		// begin writing paths.obj
		objdraw::obj_writer obj_writer("paths.obj");

		for (auto& p : obj_paths)
			obj_writer.write_path(p);

		// draw path vertices in paths.obj as icospheres
		for (auto v_0 : obj_v_0_samples) {
			objdraw::icosphere sphere(v_0, 0.15f);
			obj_writer.write_icosphere(sphere);
		}
		for (auto v : vpls) {
			objdraw::icosphere sphere(v.pos, 0.25f);
			obj_writer.write_icosphere(sphere);
		}
	}

	cout << "Avg path len: " << (1.f*vpls.size()/paths) << endl;
	cout << "max. VPL storage: " << vpl_store.size()*path_length << endl;
	vpl_stats(vpls);
}

vec3 manylight_algorithm::sample_pixel(uint32_t x, uint32_t y) {
	vec3 radiance(0);

	ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
	triangle_intersection closest = rc->scene.rt->closest_hit(view_ray);
	if (!closest.valid())
		if (rc->scene.sky) return rc->scene.sky->Le(view_ray);
		else               return vec3(0);

	diff_geom hit(closest, rc->scene);
	flip_normals_to_ray(hit, view_ray);

	// if it is a light, add the light's contribution
	if (hit.mat->emissive != vec3(0))
		return hit.mat->emissive;

	// direct illumination
	if      (sampling_mode == sample_uniform)   radiance += sample_uniformly(hit, view_ray);
	else if (sampling_mode == sample_light)     radiance += sample_lights(hit, view_ray);
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	else if (sampling_mode == sample_cosine)    radiance += sample_cosine_weighted(hit, view_ray);
	else if (sampling_mode == sample_brdf)      radiance += sample_brdfs(hit, view_ray);
#endif

	int block_size = rc->sppx / vpl_integrations;
	float vpls_per_sample = vpls.size() * (1.f/block_size);
	int vpl_index = (current_sample_index % block_size) * vpls_per_sample;
	vec3 indirect_radiance(0);

	// indirect illumination by using VPLs
	if (vpls.size() != 0) {
		for (int i = vpl_index; i < vpl_index+vpls_per_sample; ++i) {
			vpl v = vpls[i];

			auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(hit, rc->rng.uniform_float2());
			float t = length(v.pos - hit.x);

			if (!rc->scene.rt->any_hit(shadow_ray)) {
				vec3 f_x = hit.mat->brdf->f(hit, -view_ray.d, shadow_ray.d); // BRDF at x (hit)
				diff_geom v_geometry(v.is, rc->scene);
				vec3 f_v = v_geometry.mat->brdf->f(v_geometry, -shadow_ray.d, -v.w_in); // BRDF at v

				float D_x = cdot(hit.ns, shadow_ray.d); // D_x(v)
				float D_v = cdot(v.normal, -shadow_ray.d); // D_v(x)
				//float G = D_x*D_v/(t*t);
				float r = 1.5f;
				//float G = D_x*D_v/(t*t+r*r*pi*D_v);
				float attenuation = (2/(r*r)) * (1 - t/(sqrtf(t*t+r*r)));
				float G = D_x*D_v*attenuation;
				//TODO: G cap solves the issue with the bright spots but adds bias.
				//      In the future include bias compensation
				//      -> see chapter 5: bias compensation (final gathering, ...)
				//G = G > 0.1f ? 0.1f : G; //sibenik
				//G = G > 1.f ? 2.f : G; // Cornell

				indirect_radiance += f_x*G*v.col*f_v;
			}
			//if (x == 0 && y == 0)
			//	cout << i << " s: " << current_sample_index << endl;
		}
		// Scale indirect radiance to sample path
		//float avg_path_length = vpls.size() / paths;
		//indirect_radiance = indirect_radiance * (1.f/vpls_per_sample) * avg_path_length;
		float vpl_size = vpls.size();
		indirect_radiance = indirect_radiance * (1.f/vpls_per_sample) * vpl_size;
	}
	
	return radiance + indirect_radiance;
}

#ifndef RTGI_SKIP_WF
namespace wf {
	manylight_algorithm::manylight_algorithm() : direct_light() {
		//TODO-ML: manylight specific intialization
		//...
	}

	void manylight_algorithm::regenerate_steps() {
		//TODO: prevent double or triple initialization of direct_light steps
		direct_light::regenerate_steps();

		/**
		 * Manylight steps:
		 * prepare:
		 * sample_v_0s(w:v0_raydata, w:throughput/Le_v0, (r:survived), (w:obj_paths))
		 * create_vpls(r:vj_raydata, w:throughput, (r:survived), (w:obj_paths))
		 * russian_roulette(w:survived, w:throughput)
		 * copy_vpls(r:vpl_arr, w:vpls)
		 * (debug_write_obj(r:obj_paths))
		 * 
		 * integration:
		 * sample_vpls(w:ray/dir, r:vpls) <- randomly select vpl
		 * integrate_vpl_samples(r:camrays, r:shadowrays, r:pdf?, r:vpls)
		 * 
		 * Direct steps:
		 * integrate_light_sample(camdata(raydata), shadowrays(raydata), pdf)
		 * sample_uniform_dir(camdata(raydata), bouncedata(raydata), pdf)
		 * sample_cos_weighted_dir(camdata(raydata), bouncedata(raydata), pdf)
		 * (sample_light_dir(camdata(raydata), bouncedata(raydata), pdf))
		 * 
		 * Primary steps:
		 * initialize_framebuffer
		 * download_framebuffer
		 * copy_to_preview
		 * sample_camera_rays(raydata)
		 * add_hitpoint_albedo(sample_rays(raydata))
		 * find_closest_hits(raydata)
		 * find_any_hits(raydata)
		 * 
		*/

		vpl_rays = rc->platform->allocate_raydata_manually(paths, 1);
		//TODO-ML: maybe delete throughput and use data from vpls
		light_throughput = allocate_light_throughput();
		le = allocate_light_throughput();
		vpl_store = allocate_vpl_store();
		vpls = allocate_vpls();
		//sampled_vpls =  static_cast<per_sample_data<vpl>*>(rc->platform->allocate_data_per_sample(sizeof(vpl)));
		sampled_vpls = allocate_vpl_per_sample();

		/* preparation steps */
		auto *sample_v_0 = rc->platform->step<sample_v_0s>();
		sample_v_0->use(vpl_rays, light_throughput, le);
		frame_preparation_steps.push_back(sample_v_0);

		for (int depth = 0; depth < path_length; ++depth) {
			auto *find_next_hit  = rc->platform->step<find_closest_hits>("v_j hits");
			auto *create_vpl = rc->platform->step<create_vpls>("create vpls d=" + depth);
			
			find_next_hit->use(vpl_rays);
			vpl* vpl_store_lane = vpl_store+depth*paths;
			create_vpl->use(vpl_rays, light_throughput, vpl_store_lane);

			frame_preparation_steps.push_back(find_next_hit);
			frame_preparation_steps.push_back(create_vpl);
			
			if (depth >= (rr_start-1)) {
				auto *roulette = rc->platform->step<russian_roulette>();
				roulette->use(vpl_rays, light_throughput, le);
				frame_preparation_steps.push_back(roulette);
			}

			auto *sample_next_vpl = rc->platform->step<sample_next_vpls>("sample next vpl d=" + depth);
			sample_next_vpl->use(vpl_rays, light_throughput, vpl_store_lane);
			frame_preparation_steps.push_back(sample_next_vpl);
		}

		if (path_length > 0) {
			auto *copy_vpl = rc->platform->step<copy_vpls>();
			copy_vpl->use(vpl_store, vpls);
			frame_preparation_steps.push_back(copy_vpl);
		}
		else {
			// Push one vpl with col=vec3(0) that at least one vpl can be sampled
			// TODO-ML: test this
			vpls->push_back(vpl());
		}

		/* sampling steps */
		//TODO-ML: handle vpls->size() == 0 in step
		for (int i = 0; i < vpls_per_sample; ++i) {
			auto *sample_vpl = rc->platform->step<sample_vpls>();
			//auto *find_light = rc->platform->step<find_closest_hits>("secondary hits");
			auto *find_light = rc->platform->step<find_any_hits>("any hits");
			auto *integrate_vpl_sample = rc->platform->step<integrate_vpl_samples>();

			sample_vpl->use(camrays, shadowrays, vpls, sampled_vpls);
			find_light->use(shadowrays);
			integrate_vpl_sample->use(camrays, shadowrays, sampled_vpls);

			sampling_steps.push_back(sample_vpl);
			sampling_steps.push_back(find_light);
			sampling_steps.push_back(integrate_vpl_sample);
		}

		//Add prepare frame steps
		/**
		 * sample_v_0s
		 * for (int j = 1; j <= path_length; ++j) //better solutions? buffer will be empty around 40 - 70%
		 * 		find_closest_hits
		 * 		create_vpls
		 * 		russian_roulette
		 * 		sample_next_vpl
		 * 
		 * copy_vpls (not parallel!)
		 * (debug_write_obj (probably not parallel!))
		 * 
		 */

		//Add steps for indirect illumination / integration
		/**
		 * > steps from direct_light
		 * 
		 * for (int i = 0; i < vpls_per_sample; ++i) 
		 * 		sample_vpls
		 * 		find_closest_hits
		 * 		integrate_vpl_samples
		*/
	}

	vpl* manylight_algorithm::allocate_vpl_store() {
		return new vpl[paths*path_length];
	}

	vec3* manylight_algorithm::allocate_light_throughput() {
		return new vec3[paths];
	}

	vector<vpl>* manylight_algorithm::allocate_vpls() {
		return new vector<vpl>;
	}

	vpl* manylight_algorithm::allocate_vpl_per_sample() {
		ivec2 res = rc->resolution();
		return new vpl[res.x*res.y];
	}

	bool manylight_algorithm::interprete(const std::string &command, std::istringstream &in) {
		bool result_direct = direct_light::interprete(command, in);
		bool executed = false;
		while (!in.eof()) {
			string cmd;
			in >> cmd;
			if (cmd == "paths") {
				in >> paths;
				executed = true;
			}

			if (cmd == "length") {
				in >> path_length;
				executed = true;
			}

			if (cmd == "vps") {
				in >> vpls_per_sample;
				executed = true;
			}
		}

		if (executed)
			regenerate_steps();

		return result_direct || executed;
	}
}
#endif

void throughput_stats(const vec3 tp[], const int start, const int size) {
	vec3 sum(0);
	for (int i = 0; i < size; ++i)
		sum += tp[start+i];

	vec3 avg = sum * (1.0f/size);
	cout << "Test throughput (avg)   : " << avg << endl;
}

void vpl_stats(const vector<vpl>& vpls) {
	vec3 col(0);
	vec3 pos(0);
	vec3 normal(0);
	for (auto v : vpls) {
		col += v.col;
		pos += v.pos;
		normal += v.normal;
		if (col.r != col.r)
			cout << "NAN: " << v.col << endl;
	}
	col /= vpls.size();
	pos /= vpls.size();
	normal /= vpls.size();

	cout << "VPLs: " << vpls.size() << endl;
	cout << "col: " << col << ", pos: " << pos << ", normal: " << normal << endl;
}

void framebuffer_stats() {
	ivec2 res = rc->resolution();
	int size = res.x * res.y;
	vec4 col = vec4(0);
	for (int y = 0; y < res.y; ++y)
		for (int x = 0; x < res.x; ++x) {
			col += rc->framebuffer.color(x,y);
		}

	vec4 avg = col * (1.0f/size);
	cout << "Test framebuffer (avg)  : " << avg << endl;
}