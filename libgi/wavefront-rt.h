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
		virtual void run() = 0;
	};

	class platform {
	protected:
		std::string name;
		std::map<std::string, batch_ray_tracer*> tracers;
		std::map<std::string, ray_and_intersection_processing*> rnis;

		std::set<std::string> tracer_links;
		std::set<std::string> rni_links;
	public:
		platform(const std::string &name) : name(name) {}
		virtual ~platform();

		batch_ray_tracer* tracer(const std::string &name) const;
		ray_and_intersection_processing* rni(const std::string &name) const;
	};

	extern std::vector<platform*> platforms;
}
