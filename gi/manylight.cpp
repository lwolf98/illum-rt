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
static const bool export_vpl_list = false;

void manylight_algorithm::prepare_frame() {
	time_this_block(ml_preparation);
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
			float ior_water = 1.330f;
			float ior_glass = 1.520f;
			float ior_air = 1.0f;
			float current_medium_ior = ior_air;
			bool above_water = true;

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
				/*if (j >= rr_start) {
					float xi = uniform_float();
					float q = luma(throughput);
					//TODO: is this q_j or q_j+1?
					// -> equals: float q = luma(v_j.col/Le_v_0);
					
					if (xi >= q)
						break;

					throughput *= 1.0f/q;
				}
				else if (luma(throughput) == 0)
					break;*/

				// Sample ray to next VPL
				phong_specular_reflection* spec_surface = dynamic_cast<phong_specular_reflection*>(hit.mat->brdf);
				if(spec_surface != nullptr) { 
					float reflection_decision = rc->rng.uniform_float();
					if (hit.mat->roughness != 0) {
						float next_ior = 0.0f;
						if (above_water) {
							if (current_medium_ior == ior_air) {
								if (hit.mat->ior == ior_water) {
									next_ior = ior_water;
									above_water = false;
								}
								else if (hit.mat->ior == ior_glass) {
									next_ior = ior_glass;
									above_water = true;
								}
							}
							else if (current_medium_ior == ior_glass) {
								if (hit.mat->ior == ior_glass) {
									next_ior = ior_air;
									above_water = true;
								}
								else if (hit.mat->ior == ior_water) {
									next_ior = ior_water;
									above_water = false;
									to_next_vpl = ray(hit.x, to_next_vpl.d);
									closest = rc->scene.rt->closest_hit(to_next_vpl);
									if (!closest.valid()) break;
								}
							}
						}
						else {
							if (current_medium_ior == ior_water) {
								if (hit.mat->ior == ior_water) {
									next_ior = ior_air;
									above_water = true;
								}
								else if (hit.mat->ior == ior_glass) {
									next_ior = ior_glass;
									above_water = false;
								}
							}
							else if (current_medium_ior == ior_glass) {
								if (hit.mat->ior == ior_glass) {
									next_ior = ior_water;
									above_water = false;
								}
								else if (hit.mat->ior == ior_water) {
									next_ior = ior_air;
									above_water = true;
									to_next_vpl = ray(hit.x, to_next_vpl.d);
									closest = rc->scene.rt->closest_hit(to_next_vpl);
									if (!closest.valid()) break;
								}
							}
						}
						diff_geom new_hit(closest, rc->scene);
						if (new_hit.mat->emissive != vec3(0)) break;
						flip_normals_to_ray(new_hit, to_next_vpl);
						float cos_wi = dot(-to_next_vpl.d, new_hit.ns);
						vec3 new_dir(2.0f * new_hit.ns * cos_wi + to_next_vpl.d);

						float reflected_flux = fresnel_dielectric(cos_wi, current_medium_ior, next_ior);
						if (reflected_flux == 1 || reflection_decision < reflected_flux) {
							to_next_vpl = ray(new_hit.x, new_dir); 
						}
						else {
							float current_div_next = current_medium_ior/next_ior;
							vec3 refracted_dir(-current_div_next*((-to_next_vpl.d)-cos_wi*new_hit.ns)-sqrt(1-current_div_next*current_div_next*(1-cos_wi*cos_wi))*new_hit.ns);

							to_next_vpl = ray(new_hit.x, refracted_dir);
							current_medium_ior = next_ior;
						}
					}
					else {
						float cos_wi = dot(-to_next_vpl.d, hit.ns);
						vec3 new_dir(2.0f * hit.ns * cos_wi + to_next_vpl.d);
						to_next_vpl = ray(hit.x, new_dir);
					}
				}
				else {
					diff_geom v_j_geometry = hit;
					auto [w_o, f, pdf] = hit.mat->brdf->sample(v_j_geometry, -v_j.w_in, rc->rng.uniform_float2()); //f(v_j-1->v_j->v_j+1)

					// Note: 'pdf_f' does not equal 'pdf' (returned from 'sample')
					float pdf_f = hit.mat->brdf->pdf(v_j_geometry, w_o, -v_j.w_in); //p(w_j)
					to_next_vpl = ray(v_j.pos, w_o);

					// Setup the throughput for the next VPL
					float D = cdot(to_next_vpl.d, v_j.normal); //D_v_j(v_j+1)
					throughput *= D*f/pdf_f; //throughput for v_j+1
				}
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
			objdraw::icosphere sphere(v_0, 0.01f);
			obj_writer.write_icosphere(sphere);
		}
		for (auto v : vpls) {
			objdraw::icosphere sphere(v.pos, 0.02f);
			obj_writer.write_icosphere(sphere);
		}
	}

	if (export_vpl_list) {
		std::stringstream filename_stream;
		filename_stream << "vpl_list_" << vpls.size() << ".txt";
		std::ofstream out_vpls(filename_stream.str());
		out_vpls << "position;flux;normal;incoming light direction" << endl;
		for (auto v : vpls) {
			out_vpls << v.pos << ";" << v.col << ";" << v.normal << ";" << v.w_in << endl;
		}
	}

	cout << "Avg path len: " << (1.f*vpls.size()/paths) << endl;
	cout << "max. VPL storage: " << vpl_store.size()*path_length << endl;
	int block_size = rc->sppx / vpl_integrations;
	float vpls_per_sample = vpls.size() * (1.f/block_size);
	cout << "Scale: " << (1.f/vpls_per_sample) * vpls.size() << endl;

	vpls_per_sample = vpls.size() * (1.f/rc->sppx);
	cout << "VPS: " << vpls_per_sample << endl;
	cout << "Scale alternative: " << vpls.size() * (1.f/vpls_per_sample) << endl;
	vpl_stats(vpls);
}

vec3 manylight_algorithm::sample_pixel(uint32_t x, uint32_t y) {
	time_this_block(ml_integration);

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

	float ior_water = 1.330f;
	float ior_glass = 1.520f;
	float ior_air = 1.0f;
	float current_medium_ior = ior_air;
	bool above_water = true;

	lambertian_reflection* diff_surface = dynamic_cast<lambertian_reflection*>(hit.mat->brdf);
	//if (diff_surface == nullptr) return vec3(0,1,0);
	while (diff_surface == nullptr) { //|| false) {
		diff_geom spec_hit(closest, rc->scene);
		flip_normals_to_ray(spec_hit, view_ray);
		float reflection_decision = rc->rng.uniform_float();
		if (spec_hit.mat->roughness != 0) {
			float next_ior = 0.0f;
			if (above_water) {
				if (current_medium_ior == ior_air) {
					if (spec_hit.mat->ior == ior_water) {
						next_ior = ior_water;
						above_water = false;
					}
					else if (spec_hit.mat->ior == ior_glass) {
						next_ior = ior_glass;
						above_water = true;
					}
				}
				else if (current_medium_ior == ior_glass) {
					if (spec_hit.mat->ior == ior_glass) {
						next_ior = ior_air;
						above_water = true;
					}
					else if (spec_hit.mat->ior == ior_water) {
						next_ior = ior_water;
						above_water = false;
						view_ray = ray(spec_hit.x, view_ray.d);
						closest = rc->scene.rt->closest_hit(view_ray);
						if (!closest.valid()) return vec3(0);
					}
				}
			}
			else {
				if (current_medium_ior == ior_water) {
					if (spec_hit.mat->ior == ior_water) {
						next_ior = ior_air;
						above_water = true;
					}
					else if (spec_hit.mat->ior == ior_glass) {
						next_ior = ior_glass;
						above_water = false;
					}
				}
				else if (current_medium_ior == ior_glass) {
					if (spec_hit.mat->ior == ior_glass) {
						next_ior = ior_water;
						above_water = false;
					}
					else if (spec_hit.mat->ior == ior_water) {
						next_ior = ior_air;
						above_water = true;
						view_ray = ray(spec_hit.x, view_ray.d);
						closest = rc->scene.rt->closest_hit(view_ray);
						if (!closest.valid()) return vec3(0);
					}
				}
			}
			diff_geom new_hit(closest, rc->scene);
			if (new_hit.mat->emissive != vec3(0)) {
				return new_hit.mat->emissive;
			}
			flip_normals_to_ray(new_hit, view_ray);
			float cos_wi = dot(-view_ray.d, new_hit.ns);
			vec3 new_dir(2.0f * new_hit.ns * cos_wi + view_ray.d);
			
			float reflected_flux = fresnel_dielectric(cos_wi, current_medium_ior, next_ior);
			if (reflected_flux == 1 || reflection_decision < reflected_flux) {
				view_ray = ray(new_hit.x, new_dir); 
			}
			else {
				float current_div_next = current_medium_ior/next_ior;
				vec3 refracted_dir(-current_div_next*((-view_ray.d)-cos_wi*new_hit.ns)-sqrt(1-current_div_next*current_div_next*(1-cos_wi*cos_wi))*new_hit.ns);

				view_ray = ray(new_hit.x, refracted_dir);
				current_medium_ior = next_ior;
			}
		}
		else {
			float cos_wi = dot(-view_ray.d, spec_hit.ns);
			vec3 new_dir(2.0f * spec_hit.ns * cos_wi + view_ray.d);
			view_ray = ray(spec_hit.x, new_dir);
		}
		closest = rc->scene.rt->closest_hit(view_ray);
		if (!closest.valid()) return vec3(0);
		diff_geom hit_cast(closest, rc->scene);
		if (hit_cast.mat->emissive != vec3(0)) {
			return hit_cast.mat->emissive;
		}
		diff_surface = dynamic_cast<lambertian_reflection*>(hit_cast.mat->brdf);
	}
	diff_geom diff_hit(closest, rc->scene);
	flip_normals_to_ray(diff_hit, view_ray);
	//ray hit_ray = view_ray;

	// direct illumination
	if      (sampling_mode == sample_uniform)   radiance += sample_uniformly(diff_hit, view_ray);
	else if (sampling_mode == sample_light)     radiance += sample_lights(diff_hit, view_ray);
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	else if (sampling_mode == sample_cosine)    radiance += sample_cosine_weighted(diff_hit, view_ray);
	else if (sampling_mode == sample_brdf)      radiance += sample_brdfs(diff_hit, view_ray);
#endif

	int block_size = rc->sppx / vpl_integrations;
	float vpls_per_sample = vpls.size() * (1.f/block_size);
	int vpl_index = (current_sample_index % block_size) * vpls_per_sample;
	vec3 indirect_radiance(0);

	int cnt_integrated = 0;

	// indirect illumination by using VPLs
	if (vpls.size() != 0) {
		for (int i = vpl_index; i < vpl_index+vpls_per_sample; ++i) {
			vpl v = vpls[i];

			auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(diff_hit, vec2(0));
			float t = length(v.pos - diff_hit.x);

			if (!rc->scene.rt->any_hit(shadow_ray)) {
				vec3 f_x = diff_hit.mat->brdf->f(diff_hit, -view_ray.d, shadow_ray.d); // BRDF at x (hit)
				diff_geom v_geometry(v.is, rc->scene);
				vec3 f_v = v_geometry.mat->brdf->f(v_geometry, -shadow_ray.d, -v.w_in); // BRDF at v

				float D_x = cdot(diff_hit.ns, shadow_ray.d); // D_x(v)
				float D_v = cdot(v.normal, -shadow_ray.d); // D_v(x)
				float G = D_x*D_v/(t*t);
				float r = 1.5f;
				//float G = D_x*D_v/(t*t+r*r*pi*D_v);

				//new attenuation factor
				//float attenuation = (2/(r*r)) * (1 - t/(sqrtf(t*t+r*r)));
				//float G = D_x*D_v*attenuation;

				//TODO: G cap solves the issue with the bright spots but adds bias.
				//      In the future include bias compensation
				//      -> see chapter 5: bias compensation (final gathering, ...)
				//if (G > 0.0001f) cout << "triggered" << endl;
				//G = G > 0.00001f ? 0.00001f : G; //sponza
				G = G > 0.1f ? 0.1f : G; //sibenik
				//G = G > 1.f ? 1.f : G; // Cornell

				indirect_radiance += f_x*G*v.col*f_v;

				// testing
				cnt_integrated++;
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
	
	/*if (x == 153 && y == 230) {
		cout << "integrated VPLs: " << cnt_integrated << "/" << vpls.size() << endl;
		cout << "Percentage integrated VPLs: " << cnt_integrated * (1.f/vpls.size()) << endl;
		//return vec3(1.f);
	}*/

	//return vec3(0);
	return radiance + indirect_radiance;
	//return indirect_radiance;
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

		// calculate vpls per sample for strided loop (instead of configuring)
		int max_vpls_per_sample = paths*path_length/rc->sppx;
		cout << "paths: " << paths << endl;
		cout << "len: " << path_length << endl;
		cout << "SPPX: " << rc->sppx << endl;
		cout << "VPS: " << max_vpls_per_sample << endl;

		/* memory allocation */
		vpl_rays = rc->platform->allocate_raydata_manually(paths);
		//TODO-ML: maybe delete throughput and use data from vpls
		light_throughput = rc->platform->allocate_vec3_per_sample_manually(paths); //allocate_light_throughput();
		le = rc->platform->allocate_vec3_per_sample_manually(paths); //allocate_light_throughput();
		vpl_store = rc->platform->allocate_vpldata_manually(paths*path_length); //allocate_vpl_store();
		vpl_store_offset = rc->platform->allocate_int_per_sample_manually(1);
		//TODO-ML: can the size be reduced to the actual valid vpl count?
		vpls = rc->platform->allocate_vpldata_manually(paths*path_length); //vpls = rc->platform->allocate_vpls();
		vpl_count = rc->platform->allocate_int_per_sample_manually(1);
		scale = rc->platform->allocate_float_per_sample_manually(1);
		sampled_vpls = rc->platform->allocate_vpldata(); //allocate_vpl_per_sample();

		sample_index = rc->platform->allocate_int_per_sample_manually(1);
		dbg_cnt = rc->platform->allocate_int_per_sample_manually(1);
		
		// Test CUDA
		/*auto *integrate_vpl_sample = rc->platform->step<integrate_vpl_samples>();
		integrate_vpl_sample->use(camrays, shadowrays, sampled_vpls);
		sampling_steps.push_back(integrate_vpl_sample);*/

		/* preparation steps */
		auto *l_dist = rc->platform->step<compute_light_distribution>();
		//TODO-ML: data_reset_steps.push_back(l_dist); // coordinate with direct lighting depending on light or other sampling method
		auto *sample_v_0 = rc->platform->step<sample_v_0s>();
		sample_v_0->use(vpl_rays, light_throughput, le, vpl_store_offset, l_dist);
		frame_preparation_steps.push_back(sample_v_0);

		/* testing area * /
			auto *find_next_hit  = rc->platform->step<find_closest_hits>("v_j hits");
			auto *create_vpl = rc->platform->step<create_vpls>("create vpls d=" + 0);
			
			find_next_hit->use(vpl_rays);
			create_vpl->use(vpl_rays, light_throughput, vpl_store, vpl_store_offset);

			frame_preparation_steps.push_back(find_next_hit);
			frame_preparation_steps.push_back(create_vpl);

			auto *copy_vpl = rc->platform->step<copy_vpls>();
			copy_vpl->use(vpl_store, vpls, vpl_count, scale, vpls_per_sample);
			frame_preparation_steps.push_back(copy_vpl);

			/*auto *sample_vpl = rc->platform->step<sample_vpls>();
			//auto *find_light = rc->platform->step<find_closest_hits>("secondary hits");
			auto *find_light = rc->platform->step<find_any_hits>("any hits");
			auto *integrate_vpl_sample = rc->platform->step<integrate_vpl_samples>();

			sample_vpl->use(camrays, shadowrays, vpls, sampled_vpls, vpl_count);
			find_light->use(shadowrays);
			integrate_vpl_sample->use(camrays, shadowrays, sampled_vpls, scale);

			sampling_steps.push_back(sample_vpl);
			sampling_steps.push_back(find_light);
			sampling_steps.push_back(integrate_vpl_sample);* /

		/ * end testing area */

		for (int depth = 0; depth < path_length; ++depth) {
			auto *find_next_hit  = rc->platform->step<find_closest_hits>("v_j hits");
			auto *create_vpl = rc->platform->step<create_vpls>("create vpls d=" + to_string(depth));
			
			find_next_hit->use(vpl_rays);
			//vpl* vpl_store_lane = vpl_store+depth*paths;
			//create_vpl->use(vpl_rays, light_throughput, vpl_store_lane);
			create_vpl->use(vpl_rays, light_throughput, vpl_store, vpl_store_offset, depth);

			frame_preparation_steps.push_back(find_next_hit);
			frame_preparation_steps.push_back(create_vpl);
			
			/*if (depth >= (rr_start-1)) {
				auto *roulette = rc->platform->step<russian_roulette>();
				roulette->use(vpl_rays, light_throughput, le);
				frame_preparation_steps.push_back(roulette);
			}*/

			auto *sample_next_vpl = rc->platform->step<sample_next_vpls>("sample next vpl d=" + to_string(depth));
			sample_next_vpl->use(vpl_rays, light_throughput, vpl_store, vpl_store_offset, depth);
			frame_preparation_steps.push_back(sample_next_vpl);
		}

		auto *copy_vpl = rc->platform->step<copy_vpls>();
		copy_vpl->use(vpl_store, vpls, vpl_count, scale, rc->sppx);
		frame_preparation_steps.push_back(copy_vpl);

		/*if (path_length > 0) {
			auto *copy_vpl = rc->platform->step<copy_vpls>();
			copy_vpl->use(vpl_store, vpls);
			frame_preparation_steps.push_back(copy_vpl);
		}
		else {
			// Push one vpl with col=vec3(0) that at least one vpl can be sampled
			// TODO-ML: test this
			vpls->push_back(vpl());
		}*/

		/* sampling steps */
		//TODO-ML: handle vpls->size() == 0 in step
		for (int i = 0; i < max_vpls_per_sample; ++i) {
			auto *sample_vpl = rc->platform->step<sample_vpls>("prepare vpl=" + to_string(i));
			//auto *find_light = rc->platform->step<find_closest_hits>("secondary hits");
			auto *find_light = rc->platform->step<find_any_hits>("any hits");
			auto *integrate_vpl_sample = rc->platform->step<integrate_vpl_samples>("integrate vpl=" + to_string(i));

			sample_vpl->use(camrays, shadowrays, vpl_store, sampled_vpls, vpl_count, sample_index, max_vpls_per_sample, i);
			find_light->use(shadowrays);
			integrate_vpl_sample->use(camrays, shadowrays, sampled_vpls, scale, sample_index, max_vpls_per_sample, i, dbg_cnt);

			sampling_steps.push_back(sample_vpl);
			sampling_steps.push_back(find_light);
			sampling_steps.push_back(integrate_vpl_sample);
		}
	}

	/*vpl* manylight_algorithm::allocate_vpl_store() {
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
	}*/

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

void vpl_stats(const vpl* vpls, const int size) {
	vec3 col(0);
	vec3 pos(0);
	vec3 normal(0);
	//for (auto v : vpls) {
	for (int i = 0; i < size; i++) {
		vpl v = vpls[i];
		col += v.col;
		pos += v.pos;
		normal += v.normal;
		if (col.r != col.r)
			cout << "NAN: " << v.col << endl;
	}
	col /= size;
	pos /= size;
	normal /= size;

	cout << "VPLs: " << size << endl;
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