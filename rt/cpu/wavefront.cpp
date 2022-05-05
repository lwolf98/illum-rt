#include "wavefront.h"
#include "platform.h"
#include "libgi/timer.h"

#include "seq.h"
#include "bvh.h"

#include "config.h"

#ifdef HAVE_LIBEMBREE3
#include "embree.h"
#endif

#include <iostream>

#define check_in(x) { if (in.bad() || in.fail()) std::cerr << "error in command: " << (x) << std::endl; }

namespace wf {
	namespace cpu {

		// raydata

		raydata::raydata(int w, int h) : w(w), h(h) {
			if (w > 0 && h > 0) {
				rays = new ray[w*h];
				intersections = new triangle_intersection[w*h];
			}
			rc->call_at_resolution_change[this] = [this](int new_w, int new_h) {
				delete [] rays;
				delete [] intersections;
				this->w = new_w;
				this->h = new_h;
				rays = new ray[this->w*this->h];
				intersections = new triangle_intersection[this->w*this->h];
			};
		}
		raydata::~raydata() {
			rc->call_at_resolution_change.erase(this);
			delete [] rays;
			delete [] intersections;
		}

		// batch_rt_adapter

		void batch_rt_adapter::compute_closest_hit() {
			glm::ivec2 res = rc->resolution();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)  // ray data missing
					rd.intersections[y*res.x+x] = underlying_rt->closest_hit(rd.rays[y*res.x+x]);
		}
		void batch_rt_adapter::compute_any_hit() {
			glm::ivec2 res = rc->resolution();	
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)
					rd.intersections[y*res.x+x] = underlying_rt->any_hit(rd.rays[y*res.x+x]);
		}

		void batch_rt_adapter::build(cpu::scene *s) {
			underlying_rt->build(s);
		}

		// RNI STEPS

		void initialize_framebuffer::run() {
			time_this_wf_step;
			auto res = rc->resolution();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)
					rc->framebuffer.color(x,y) = vec4(0,0,0,0);
		}

		void batch_cam_ray_setup_cpu::run() {
			time_this_wf_step;
			auto res = rc->resolution();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					ray view_ray = cam_ray(pf->scene->camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
					pf->rt->rd.rays[y*res.x+x] = view_ray;
				}
		}
			
		// store_hitpoint_albedo

		void add_hitpoint_albedo::run() {
			time_this_wf_step;
			auto res = rc->resolution();
			float one_over_samples = 1.0f/rc->sppx;
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x) {
					vec3 radiance(0);
					for (int sample = 0; sample < rc->sppx; ++sample) {
						triangle_intersection closest = pf->rt->rd.intersections[y*res.x+x];
						if (closest.valid()) {
							diff_geom dg(closest, *pf->scene);
							radiance += dg.albedo();
						}
					}
					radiance *= one_over_samples;
					rc->framebuffer.color(x,y) += vec4(radiance, 1);
				}
		}
			
		void download_framebuffer::run() {
			time_this_wf_step;
			auto res = rc->resolution();
			#pragma omp parallel for
			for (int y = 0; y < res.y; ++y)
				for (int x = 0; x < res.x; ++x)
					rc->framebuffer.color(x,y) /= rc->framebuffer.color(x,y).w;
		}

		find_closest_hits::find_closest_hits() : wf::find_closest_hits(pf->rt) {
		}

		find_any_hits::find_any_hits() : wf::find_any_hits(pf->rt) {
		}

		// THE PLATFORM

		platform::platform(const std::vector<std::string> &args) : wf::platform("cpu") {
			if (pf) std::logic_error("The " + name + " platform is already set up");
			pf = this;

			for (auto arg : args)
				std::cerr << "Platform opengl does not support the argument " << arg << std::endl;
			cpu::raydata *rd = new cpu::raydata(rc->resolution());
			raydata = rd;

			register_batch_rt("seq", rd, batch_rt_adapter(new seq_tri_is, rd));
			register_batch_rt("bbvh-esc", rd, batch_rt_adapter(new binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>, rd));

#ifdef HAVE_LIBEMBREE3
			register_batch_rt("embree", rd, batch_rt_adapter(new embree_tracer, rd));
			link_tracer("embree", "default");
#else
#ifndef RTGI_SKIP_BVH
			link_tracer("bbvh-esc", "default");
#else
			link_tracer("seq", "default");
#endif
#endif

			// bvh mode?
			register_wf_step_by_id(, initialize_framebuffer);
			register_wf_step_by_id(, batch_cam_ray_setup_cpu);
			register_wf_step_by_id(, add_hitpoint_albedo);
			register_wf_step_by_id(, download_framebuffer);
			register_wf_step_by_id(, find_closest_hits);
			register_wf_step_by_id(, find_any_hits);

			timer = new wf::cpu::timer;
		}
		
		platform::~platform() {
			pf = nullptr;
		}

		void platform::commit_scene(cpu::scene *scene) {
			if (!rt)
				rt = dynamic_cast<batch_rt*>(select("default"));
			this->scene = scene;
			scene->compute_light_distribution(); // TODO extract as step
			rt->build(scene);
		}
	
		bool platform::interprete(const std::string &command, std::istringstream &in) { 
			if (command == "raytracer") {
				std::string variant;
				in >> variant;
				check_in("Syntax error, requires opengl ray tracer variant name");
				rt = dynamic_cast<batch_rt*>(select(variant));
				return true;
			}
			return false;
		}

		platform *pf = nullptr;
	}
}
