#pragma once

#include "libgi/wavefront-rt.h"

#include "gi/primary-hit.h"

namespace wf {
	//! Simple CPU implementation of wavefront style ray tracing primitives
	namespace cpu {

		struct raydata : public wf::raydata {
			int w, h;
			ray *rays = nullptr;
			triangle_intersection *intersections = nullptr;

			raydata(glm::ivec2 dim) : raydata(dim.x, dim.y) {}
			raydata(int w, int h);
			~raydata();
		};

		struct batch_rt : public batch_ray_tracer {
			raydata &rd;
			batch_rt(raydata *rd) : rd(*rd) {
			}
		};

		class batch_rt_adapter : public batch_rt {
		protected:
			individual_ray_tracer *underlying_rt = nullptr;
		public:
			batch_rt_adapter(individual_ray_tracer *underlying_rt, raydata *rd) : batch_rt(rd), underlying_rt(underlying_rt) {
			}
			~batch_rt_adapter() {
				delete underlying_rt;
			}
			void compute_closest_hit() override;
			void compute_any_hit() override;
			void build(::scene *s) override;
		};

		/*! \brief Computation nodes for managing Rays and Intersections, aka computing Bounces
		 *
		 */
		template<typename T> struct rni : public T {
			batch_rt *rt;	// most common base class possible to have the proper ray and scene layout
			                // might have to be moved to derived classes
			void use(wf::batch_ray_tracer *that) override { 
				rt = dynamic_cast<cpu::batch_rt*>(that); 
			}
		};

		struct initialize_framebuffer : public rni<wf::initialize_framebuffer> {
			void run() override;
		};
			
		struct batch_cam_ray_setup_cpu : public rni<wf::sample_camera_rays> {
			void run() override;
		};
		
		struct add_hitpoint_albedo : public rni<wf::add_hitpoint_albedo> {
			void run() override;
		};
		
		struct download_framebuffer : public rni<wf::download_framebuffer> {
			void run() override;
		};
	
	}
}
