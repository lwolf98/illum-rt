#include "wavefront-rt.h"

namespace wf {
	platform::~platform() {
		// remove the links (aka duplicate pointers)
		for (auto x : tracer_links)
			tracers.erase(tracers.find(x));
		for (auto x : rni_links)
			rnis.erase(rnis.find(x));
		delete raydata;
	}
		
	void platform::link_tracer(const std::string &existing, const std::string &linkname) {
		tracer_links.insert(linkname);
		tracers[linkname] = tracers[existing];
	}

	batch_ray_tracer* platform::select(const std::string &name) {
		if (auto it = generated_tracers.find(name); it != generated_tracers.end())
			selected_tracer = it->second;
		else {
			selected_tracer = tracers[name]();
			generated_tracers[name] = selected_tracer;
		}
		for (auto [_,r] : generated_rnis)
			r->use(selected_tracer);
		return selected_tracer;
	}

	ray_and_intersection_processing* platform::rni(const std::string &name) {
		if (auto it = generated_rnis.find(name); it != generated_rnis.end())
			return it->second;

		ray_and_intersection_processing *r = rnis[name]();
		r->use(selected_tracer);
		generated_rnis[name] = r;
		return r;
	}

	std::vector<platform*> platforms;
}
