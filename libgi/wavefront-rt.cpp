#include "wavefront-rt.h"

namespace wf {
	platform::~platform() {
		// remove the links (aka duplicate pointers)
		for (auto x : tracer_links)
			tracers.erase(tracers.find(x));
		for (auto x : step_links)
			steps.erase(steps.find(x));
// 		delete raydata;
		delete timer;
	}
		
	void platform::link_tracer(const std::string &existing, const std::string &linkname) {
		tracer_links.insert(linkname);
		tracers[linkname] = tracers[existing];
	}

	batch_ray_tracer* platform::select(const std::string &name) {
		if (auto it = generated_tracers.find(name); it != generated_tracers.end())
			selected_tracer = it->second;
		else {
			if (tracers.count(name) == 0)
				throw std::logic_error(std::string("No tracer ") + name + " for platform " + this->name);
			selected_tracer = tracers[name]();
			generated_tracers[name] = selected_tracer;
		}
		return selected_tracer;
	}

	wf::step* platform::step(const std::string &id, std::string name) {
		if (name == "") name = id;
		if (auto it = generated_steps.find(name); it != generated_steps.end())
			return it->second;

		if (steps.count(id) == 0)
			return nullptr;

		wf::step *s = steps[id]();
// 		r->use(selected_tracer);
		generated_steps[name] = s;
		return s;
	}

	std::vector<platform*> platforms;
}
