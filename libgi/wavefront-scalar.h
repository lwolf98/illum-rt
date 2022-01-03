#pragma once

#include "wavefront-rt.h"

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

		struct batch_ray_and_intersection_processing_cpu : public ray_and_intersection_processing {
			batch_rt *rt;
			virtual void use(batch_ray_tracer *that) override { rt = dynamic_cast<cpu::batch_rt*>(that); }
			virtual void run() = 0;
		};

		struct batch_cam_ray_setup_cpu : public batch_ray_and_intersection_processing_cpu {
			void run() override;
		};
		
		struct store_hitpoint_albedo : public batch_ray_and_intersection_processing_cpu {
			void run() override;
		};

		class scalar_cpu_batch_raytracing : public platform {
		public:
			scalar_cpu_batch_raytracing();
		};
	}
}
