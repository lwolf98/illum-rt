#pragma once

#include "rt.h"

#include "global-context.h"
#include "context.h"
#include "camera.h"

#include <map>
#include <set>

namespace wf {

	// partition in pure interface and templated version holding a reference to the data
	struct batch_ray_tracer : public ray_tracer {
		ray *rays = nullptr;
		triangle_intersection *intersections = nullptr;

		virtual void compute_closest_hit() = 0;
		virtual void compute_any_hit() = 0;
	};
	
	struct ray_and_intersection_processing {
		virtual void use(batch_ray_tracer *rt) = 0;
		virtual void run() = 0;
	};

	struct raydata {
		virtual ~raydata() {}
	};

	#define register_batch_rt(N,X) tracers[N] = [&]() -> wf::batch_ray_tracer* { return new X; }
	#define register_rni_step(N,X) rnis[N] = [&]() -> wf::ray_and_intersection_processing* { return new X; }
	class platform {
	protected:
		std::string name;
		std::map<std::string, std::function<batch_ray_tracer*()>> tracers;
		std::map<std::string, std::function<ray_and_intersection_processing*()>> rnis;

		std::set<std::string> tracer_links;
		std::set<std::string> rni_links;

		std::map<std::string, batch_ray_tracer*> generated_tracers;
		std::map<std::string, ray_and_intersection_processing*> generated_rnis;

		void link_tracer(const std::string &existing, const std::string &linkname);
	public:
		platform(const std::string &name) : name(name) {}
		virtual ~platform();

		wf::raydata *raydata = nullptr;
		batch_ray_tracer *selected_tracer = nullptr;

		batch_ray_tracer* select(const std::string &name);
		ray_and_intersection_processing* rni(const std::string &name);
	};

	extern std::vector<platform*> platforms;
}
