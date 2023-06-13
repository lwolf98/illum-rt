#include "manylight.h"

#include "libgi/util.h"
#include "libgi/color.h"

#include <iostream>
using namespace std;

static bool pointlight_attenuation = false;

namespace wf::cpu {
	void sample_v_0s::run() {
		time_this_wf_step;
		if (!dynamic_cast<manylight_algorithm*>(rc->algo)) {
			//TODO: better handling for this situation
			return;
		}
		manylight_algorithm* ml = dynamic_cast<manylight_algorithm*>(rc->algo);
		auto paths = ml->get_paths();
		#pragma omp parallel for
		for (int i = 0; i < paths; ++i) {
			auto [l_id, pdf_l] = rc->scene.light_distribution->sample_index(rc->rng.uniform_float());
			light *l = rc->scene.lights[l_id];
			vec2 xis_pos = rc->rng.uniform_float2();
			vec2 xis_dir = rc->rng.uniform_float2();
			auto [to_next_vpl, Le_v_0, normal_v_0, pdf_Le] = l->sample_Le(xis_pos, xis_dir);
			float pdf_v_0 = pdf_l * pdf_Le;
			
			//obj_v_0_samples[i] = to_next_vpl.o;
			//objdraw::path obj_path(to_next_vpl.o);

			// Setup the throughput for VPL v_1
			float D_v_0 = cdot(normal_v_0, to_next_vpl.d); //D_v_0(v_1)
			vec3 throughput(1);
			throughput *= Le_v_0 * D_v_0 / pdf_v_0; //Attention: throughput contains Le value

			light_rays->rays[i] = to_next_vpl;
			light_throughput->data[i] = throughput;
			le->data[i] = Le_v_0;

			//Initialize VPL store offset
			vpl_store_offset->data[0] = 0;
		}
	}

	void create_vpls::run() {
		time_this_wf_step;
		if (!dynamic_cast<manylight_algorithm*>(rc->algo)) {
			//TODO: better handling for this situation
			return;
		}
		manylight_algorithm* ml = dynamic_cast<manylight_algorithm*>(rc->algo);
		auto paths = ml->get_paths();

		#pragma omp parallel for
		for (int i = 0; i < paths; ++i) {
			// check if light ray is valid else continue
			if (light_rays->rays[i].d == vec3(0))
				continue;

			triangle_intersection is = light_rays->intersections[i];
			if (!is.valid())
				continue;

			diff_geom hit(is, *pf->sd);
			vec3 col = light_throughput->data[i];

			vec3 pos = hit.x;
			vec3 normal = hit.ns;
			vec3 w_in = light_rays->rays[i].d;
			vpl v(col*(1.f/paths), pos, normal, w_in, is);
			//cout << "VPL: " << col << endl;
			//int offset = vpl_store_offset->data[0];
			int offset = depth*paths;
			vpl_store->vpls[i+offset] = v;
		}
		//cout << "finished: create_vpls" << endl;
	}

	void russian_roulette::run() {
		time_this_wf_step;
		if (!dynamic_cast<manylight_algorithm*>(rc->algo)) {
			//TODO: better handling for this situation
			return;
		}
		manylight_algorithm* ml = dynamic_cast<manylight_algorithm*>(rc->algo);
		auto paths = ml->get_paths();

		#pragma omp parallel for
		for (int i = 0; i < paths; ++i) {
			// check if light ray is valid else continue
			if (light_rays->rays[i].d == vec3(0))
				continue;
			
			if (luma(light_throughput->data[i]) == 0) {
				light_rays->rays[i].d = vec3(0);
				continue;
			}

			float xi = rc->rng.uniform_float();
			float q = luma(light_throughput->data[i]/le->data[i]);
			//TODO: is this q_j or q_j+1?
			// -> equals: float q = luma(v_j.col/Le_v_0);
			
			if (xi >= q) {
				light_rays->rays[i].d = vec3(0);
				continue;
			}

			light_throughput->data[i] *= 1.0f/q;
		}
	}

	void sample_next_vpls::run() {
		time_this_wf_step;
		if (!dynamic_cast<manylight_algorithm*>(rc->algo)) {
			//TODO: better handling for this situation
			return;
		}
		manylight_algorithm* ml = dynamic_cast<manylight_algorithm*>(rc->algo);
		auto paths = ml->get_paths();

		#pragma omp parallel for
		for (int i = 0; i < paths; ++i) {
			// check if path has terminated
			if (light_rays->rays[i].d == vec3(0))
				continue;

			//int offset = vpl_store_offset->data[0];
			int offset = depth*paths;
			vpl v = vpl_store->vpls[i+offset];
			// check if light ray is valid else continue
			if (!v.is.valid()) {
				light_rays->rays[i].d = vec3(0);
				continue;
			}

			// Sample ray to next VPL
			diff_geom v_geometry(v.is, *pf->sd);
			auto [w_o, f, pdf] = v_geometry.mat->brdf->sample(v_geometry, -v.w_in, rc->rng.uniform_float2()); //f(v_j-1->v_j->v_j+1)

			// Note: 'pdf_f' does not equal 'pdf' (returned from 'sample')
			//pdf = v_geometry.mat->brdf->pdf(v_geometry, w_o, -v.w_in); //p(w_j)
			ray to_next_vpl(v.pos, w_o);
			light_rays->rays[i] = to_next_vpl;

			// Setup the throughput for the next VPL
			float D = cdot(to_next_vpl.d, v_geometry.ns); //v.normal); //D_v_j(v_j+1)
			light_throughput->data[i] *= D*f/pdf; //throughput for v_j+1
		}

		//TODO-ML: how to uptade the offset in CUDA? won't work this way...
		vpl_store_offset->data[0] += paths;
	}

	static distribution_1d *vpl_dist;
	void copy_vpls::run() {
		time_this_wf_step;
		if (!dynamic_cast<manylight_algorithm*>(rc->algo)) {
			//TODO: better handling for this situation
			return;
		}
		manylight_algorithm* ml = dynamic_cast<manylight_algorithm*>(rc->algo);
		auto paths = ml->get_paths();
		auto path_length = ml->get_path_length();

		int count = 0;
		for (int depth = 0; depth < path_length; ++depth) {
			for (int path = 0; path < paths; ++path) {
				//vpls->vpls[count] = vpl();
				vpl v = vpl_store->vpls[depth*paths+path];
				vpl_store->vpls[depth*paths+path] = vpl();
				if (v.col != vec3(0) && v.is.valid()) {
					//vpls->vpls[count] = v; //push_back(v);
					vpl_store->vpls[count] = v;
					vpls->vpls[count] = v;
					count++;
				}
			}
		}
		vpl_count->data[0] = count;
		scale->data[0] = count;
		rc->vpl_count = count;

		// build vpl distribution
		cout << "build distribution" << endl;
		vector<float> power;
		for (int i = 0; i < count; i++) {
			//cout << "Add power: " << luma(vpls->vpls[i].power()) << endl;
			power.push_back(luma(vpls->vpls[i].col));
		}

		cout << "distribution finished" << endl;
		vpl_dist = new distribution_1d(power);

		cout << "Pos.: " << pf->sd->camera.pos << ", Dir.: " << pf->sd->camera.dir << endl;
		cout << "max. VPL storage: " << paths*path_length << endl;
		vpl_stats(vpls->vpls, count);
	}

	static float tmp_pdf[1000000];
	int index = 0;
	void sample_vpls::run() {
		time_this_wf_step;
		auto res = rc->resolution();
		#pragma omp parallel for
		for (int y = 0; y < res.y; ++y)
			for (int x = 0; x < res.x; ++x) {
				triangle_intersection is_camray = camrays->intersections[y*res.x+x];
				diff_geom hit(is_camray, *pf->sd);
				//TODO-ML: how to work with vpls->size()?
				//int32_t pos = rc->rng.uniform_float() * vpls->size();
				//int32_t pos = rc->rng.uniform_float() * vpl_count->data[0];
				//int32_t pos = index;
				auto [pos, pos_pdf] = vpl_dist->sample_index(rc->rng.uniform_float());
				//cout << "PDF: " << pos_pdf << endl;
				//tmp_pdf[y*res.x+x] = pos_pdf;
				//pos = 2;
				//TODO-ML: better names: vpls->vpls[pos];
				vpl v = vpls->vpls[pos];
				//cout << pos << ": " << v.col << " " << v.pos << " " << v.w_in << endl;

				// discard col and pdf; pdf is also wrong because vpl does not override pointlight::sample_Li yet
				//auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(hit, rc->rng.uniform_float2());
				auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(hit, vec2(0));
				diff_geom v_geom(v.is, *pf->sd);

				shadowrays->rays[y*res.x+x] = shadow_ray;
				//sampled_vpls->vpls[y*res.x+x] = v;
				sampled_vpl_indices->data[y*res.x+x] = pos;
			}
	}

	void integrate_vpl_samples::run() {
		time_this_wf_step;
		if (!dynamic_cast<manylight_algorithm*>(rc->algo)) {
			//TODO: better handling for this situation
			return;
		}
		manylight_algorithm* ml = dynamic_cast<manylight_algorithm*>(rc->algo);
		auto paths = ml->get_paths();
		float vps = ml->get_vpls_per_sample();
		//float vpl_count = ml->get_vpl_count();
		float vpl_count = scale->data[0];
		index++;

		auto res = rc->resolution();
		#pragma omp parallel for
		for (int y = 0; y < res.y; ++y)
			for (int x = 0; x < res.x; ++x) {
				//TMP: Basic test
				/*rc->framebuffer.color(x,y) += vec4(0, 1, 0, 0);
				continue;*/

				vec3 radiance(0);
				ray cam_ray = camrays->rays[y*res.x+x];
				triangle_intersection is_x = camrays->intersections[y*res.x+x];
				diff_geom hit(is_x, *pf->sd);

				ray shadow_ray = shadowrays->rays[y*res.x+x];
				triangle_intersection is_test = shadowrays->intersections[y*res.x+x];
				//vpl v = sampled_vpls->vpls[y*res.x+x];
				int32_t vpl_index = sampled_vpl_indices->data[y*res.x+x];
				vpl v = vpls->vpls[vpl_index];

				//TODO-ML: Does this need to be checked when using any_hits?
				//bool valid_shadowray = (shadowrays->rays[y*res.x+x].t_max > 0);
				if (is_x.valid() && !is_test.valid()) {
					diff_geom v_geometry(v.is, *pf->sd);
					float t = length(v_geometry.x - hit.x);

					vec3 f_x = hit.mat->brdf->f(hit, -cam_ray.d, shadow_ray.d); // BRDF at x (hit)
					vec3 f_v = v_geometry.mat->brdf->f(v_geometry, -shadow_ray.d, -v.w_in); // BRDF at v
					float D_x = cdot(hit.ns, shadow_ray.d); // D_x(v)
					float D_v = cdot(v_geometry.ns, -shadow_ray.d); // D_v(x)
					//float G = D_x*D_v/(t*t);
					//TODO: G cap solves the issue with the bright spots but adds bias.
					//      In the future include bias compensation
					//      -> see chapter 5: bias compensation (final gathering, ...)
					//G = G > 0.1f ? 0.1f : G; // sibenik
					//G = G > 1.f ? 1.f : G; // cornell
					float G;
					if (pointlight_attenuation) {
						float r = G_max;
						//attenuation factor
						float attenuation = (2/(r*r)) * (1 - t/(sqrtf(t*t+r*r)));
						G = D_x*D_v*attenuation;
					}
					else {
						G = D_x*D_v/(t*t);
						G = G > G_max ? G_max : G;
					}

					radiance = f_x*G*v.col*f_v;

					//Scale to part of avg path length
					//float avg_path_len = vpl_count / paths;
					float avg_path_len = vpl_count; //TODO-ML: revert to line above
					radiance = radiance * avg_path_len/vps;
					//radiance = radiance * scale->data[0];
					//cout << "count: " << avg_path_len << ", vps: " << vps << endl;
					//radiance = radiance / tmp_pdf[y*res.x+x];
					//cout << "scale: " << avg_path_len/vps << endl;
				}
				rc->framebuffer.color(x,y) += vec4(radiance, 0);
				//rc->framebuffer.color(x,y) = vec4(radiance, 1.f);
			}
	}
}