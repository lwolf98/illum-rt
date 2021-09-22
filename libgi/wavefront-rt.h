#include "rt.h"

#include "global-context.h"
#include "context.h"
#include "camera.h"

namespace wf {

	// partition in pure interface and templated version holding a reference to the data
	struct batch_ray_tracer : public ray_tracer {
		ray *rays = nullptr;
		triangle_intersection *intersections = nullptr;

		virtual void compute_closest_hit() = 0;
		virtual void compute_any_hit() = 0;
	};
	
	struct ray_and_intersection_processing {
	};

	//! Simple CPU implementation of wavefront style ray tracing primitives
	namespace cpu {

		struct raydata {
			int w, h;
			ray *rays = nullptr;
			triangle_intersection *intersections = nullptr;

			raydata(glm::ivec2 dim) : raydata(dim.x, dim.y) {
			}
			raydata(int w, int h) : w(w), h(h) {
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
			~raydata() {
				rc->call_at_resolution_change.erase(this);
				delete [] rays;
				delete [] intersections;
			}
		};

		struct batch_rt : public batch_ray_tracer {
			raydata rd;
			batch_rt() : rd(rc->resolution()) {
			}
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
			void compute_closest_hit() override {
				glm::ivec2 res = rc->resolution();	
				#pragma omp parallel for
				for (int y = 0; y < res.y; ++y)
					for (int x = 0; x < res.x; ++x)  // ray data missing
						rd.intersections[y*res.x+x] = underlying_rt->closest_hit(rd.rays[y*res.x+x]);
			}
			void compute_any_hit() override {
				glm::ivec2 res = rc->resolution();	
				#pragma omp parallel for
				for (int y = 0; y < res.y; ++y)
					for (int x = 0; x < res.x; ++x)
						rd.intersections[y*res.x+x] = underlying_rt->any_hit(rd.rays[y*res.x+x]);
			}
			void build(::scene *s) override {
				underlying_rt->build(s);
			}
		};

		struct batch_ray_and_intersection_processing_cpu : public ray_and_intersection_processing {
			virtual void run() = 0;
		};

		struct batch_cam_ray_setup_cpu : public batch_ray_and_intersection_processing_cpu {
			void run() override {
				auto res = rc->resolution();
				auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
				assert(rt != nullptr);
				#pragma omp parallel for
				for (int y = 0; y < res.y; ++y)
					for (int x = 0; x < res.x; ++x) {
						ray view_ray = cam_ray(rc->scene.camera, x, y, glm::vec2(rc->rng.uniform_float()-0.5f, rc->rng.uniform_float()-0.5f));
						rt->rd.rays[y*res.x+x] = view_ray;
					}
			}
		};

	}
}
