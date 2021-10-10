#include "wavefront-rt.h"

namespace wf {
	platform::~platform() {
		// remove the links (aka duplicate pointers)
		for (auto x : tracer_links)
			tracers.erase(tracers.find(x));
		for (auto x : rni_links)
			rnis.erase(rnis.find(x));
	}

	batch_ray_tracer* platform::tracer(const std::string &name) const {
		auto it = tracers.find(name);
		if (it != tracers.end()) return it->second;
		throw std::logic_error("There is no tracer called '" + name + "' on platform '" + this->name + "'");
	}

	ray_and_intersection_processing* platform::rni(const std::string &name) const {
		auto it = rnis.find(name);
		if (it != rnis.end()) return it->second;
		throw std::logic_error("There is no rni step called '" + name + "' on platform '" + this->name + "'");
	}

	std::vector<platform*> platforms;
}
