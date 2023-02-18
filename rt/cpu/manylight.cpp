#include "manylight.h"

#include "libgi/util.h"
#include "libgi/color.h"

#include <iostream>
using namespace std;

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
			light_throughput[i] = throughput;
			le[i] = Le_v_0;
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
			diff_geom hit(is, *pf->sd);
			vec3 col = light_throughput[i];
			vec3 pos = hit.x;
			vec3 normal = hit.ns;
			vec3 w_in = light_rays->rays[i].d;
			vpl v(col, pos, normal, w_in, is);
			vpl_store_lane[i] = v;
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
			
			if (luma(light_throughput[i]) == 0) {
				light_rays->rays[i].d = vec3(0);
				continue;
			}

			float xi = rc->rng.uniform_float();
			float q = luma(light_throughput[i]/le[i]);
			//TODO: is this q_j or q_j+1?
			// -> equals: float q = luma(v_j.col/Le_v_0);
			
			if (xi >= q) {
				light_rays->rays[i].d = vec3(0);
				continue;
			}

			light_throughput[i] *= 1.0f/q;
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
			// check if light ray is valid else continue
			if (light_rays->rays[i].d == vec3(0))
				continue;

			vpl v = vpl_store_lane[i];

			// Sample ray to next VPL
			diff_geom v_geometry(v.is, *pf->sd);
			auto [w_o, f, pdf] = v_geometry.mat->brdf->sample(v_geometry, -v.w_in, rc->rng.uniform_float2()); //f(v_j-1->v_j->v_j+1)

			// Note: 'pdf_f' does not equal 'pdf' (returned from 'sample')
			float pdf_f = v_geometry.mat->brdf->pdf(v_geometry, w_o, -v.w_in); //p(w_j)
			ray to_next_vpl(v.pos, w_o);
			light_rays->rays[i] = to_next_vpl;

			// Setup the throughput for the next VPL
			float D = cdot(to_next_vpl.d, v_geometry.ns); //v.normal); //D_v_j(v_j+1)
			light_throughput[i] *= D*f/pdf_f; //throughput for v_j+1
		}
	}

	void copy_vpls::run() {
		time_this_wf_step;
		if (!dynamic_cast<manylight_algorithm*>(rc->algo)) {
			//TODO: better handling for this situation
			return;
		}
		manylight_algorithm* ml = dynamic_cast<manylight_algorithm*>(rc->algo);
		auto paths = ml->get_paths();
		auto path_length = ml->get_path_length();

		for (int depth = 0; depth < path_length; ++depth) {
			for (int path = 0; path < paths; ++path) {
				vpl v = vpl_store[depth*paths+path];
				if (v.col != vec3(0))
					vpls->push_back(v);
			}
		}

		cout << "max. VPL storage: " << paths*path_length << endl;
		vpl_stats(*vpls);
	}

	void sample_vpls::run() {
		time_this_wf_step;
		auto res = rc->resolution();
		#pragma omp parallel for
		for (int y = 0; y < res.y; ++y)
			for (int x = 0; x < res.x; ++x) {
				triangle_intersection is_camray = camrays->intersections[y*res.x+x];
				diff_geom hit(is_camray, *pf->sd);
				int32_t pos = rc->rng.uniform_float() * vpls->size();
				vpl v = (*vpls)[pos];

				// discard col and pdf; pdf is also wrong because vpl does not override pointlight::sample_Li yet
				auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(hit, rc->rng.uniform_float2());
				diff_geom v_geom(v.is, *pf->sd);

				shadowrays->rays[y*res.x+x] = shadow_ray;
				sampled_vpls[y*res.x+x] = v;
			}
	}

	void integrate_vpl_samples::run() {
		time_this_wf_step;
		auto res = rc->resolution();
		#pragma omp parallel for
		for (int y = 0; y < res.y; ++y)
			for (int x = 0; x < res.x; ++x) {
				vec3 radiance(0);
				ray cam_ray = camrays->rays[y*res.x+x];
				triangle_intersection is_x = camrays->intersections[y*res.x+x];
				diff_geom hit(is_x, *pf->sd);

				ray shadow_ray = shadowrays->rays[y*res.x+x];
				triangle_intersection is_test = shadowrays->intersections[y*res.x+x];
				vpl v = sampled_vpls[y*res.x+x];

				if (is_x.valid() && !is_test.valid()) {
					diff_geom v_geometry(v.is, *pf->sd);
					float t = length(v_geometry.x - hit.x);

					vec3 f_x = hit.mat->brdf->f(hit, -cam_ray.d, shadow_ray.d); // BRDF at x (hit)
					vec3 f_v = v_geometry.mat->brdf->f(v_geometry, -shadow_ray.d, -v.w_in); // BRDF at v
					float D_x = cdot(hit.ns, shadow_ray.d); // D_x(v)
					float D_v = cdot(v_geometry.ns, -shadow_ray.d); // D_v(x)
					float G = D_x*D_v/(t*t);
					//TODO: G cap solves the issue with the bright spots but adds bias.
					//      In the future include bias compensation
					//      -> see chapter 5: bias compensation (final gathering, ...)
					G = G > 0.1f ? 0.1f : G;

					radiance = f_x*G*v.col*f_v;
				}
				rc->framebuffer.color(x,y) += vec4(radiance, 0);
			}
	}
}