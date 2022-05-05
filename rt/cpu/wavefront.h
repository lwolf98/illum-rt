#pragma once

#include "libgi/wavefront-rt.h"

#include "gi/primary-hit.h"

namespace wf {
	//! Simple CPU implementation of wavefront style ray tracing primitives
	namespace cpu {
	
		class platform;
		typedef ::scene scene;

		struct timer : public wf::timer {
			void start(const std::string &name) override { stats_timer.start(name); }
			void stop(const std::string &name) override { stats_timer.stop(name); }
			void synchronize() override {}
		};

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
			void build(cpu::scene *s) override;
		};

		struct initialize_framebuffer : public wf::initialize_framebuffer {
			void run() override;
		};
			
		struct batch_cam_ray_setup_cpu : public wf::sample_camera_rays {
			void run() override;
		};
		
		struct add_hitpoint_albedo : public wf::add_hitpoint_albedo {
			void run() override;
		};
		
		struct download_framebuffer : public wf::download_framebuffer {
			void run() override;
		};
	
		struct find_closest_hits : public wf::find_closest_hits {
			find_closest_hits();
		};

		struct find_any_hits : public wf::find_any_hits {
			find_any_hits();
		};
	
	}
}
