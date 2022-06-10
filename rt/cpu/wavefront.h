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
			raydata *rd = nullptr;
			virtual void build(cpu::scene *s) = 0;
			void use(wf::raydata *rays) override { rd = dynamic_cast<raydata*>(rays); }
		};

		class batch_rt_adapter : public batch_rt {
		protected:
			individual_ray_tracer *underlying_rt = nullptr;
		public:
			batch_rt_adapter(individual_ray_tracer *underlying_rt) : underlying_rt(underlying_rt) {
			}
			~batch_rt_adapter() {
				delete underlying_rt;
			}
			void compute_closest_hit() override;
			void compute_any_hit() override;
			virtual void build(cpu::scene *s);
		};

		struct initialize_framebuffer : public wf::wire::initialize_framebuffer<raydata> {
			void run() override;
		};
			
		struct batch_cam_ray_setup_cpu : public wf::wire::sample_camera_rays<raydata> {
			void run() override;
		};
		
		struct add_hitpoint_albedo : public wf::wire::add_hitpoint_albedo<raydata> {
			void run() override;
		};
		
		struct download_framebuffer : public wf::wire::download_framebuffer<raydata> {
			void run() override;
		};
	
		struct find_closest_hits : public wf::wire::find_closest_hits<raydata> {
			find_closest_hits();
		};

		struct find_any_hits : public wf::wire::find_any_hits<raydata> {
			find_any_hits();
		};
	
	}
}
