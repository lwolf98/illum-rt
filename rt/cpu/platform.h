#pragma once

#include "wavefront.h"

#include "libgi/wavefront-rt.h"

class scene;

namespace wf::cpu {

	class platform : public wf::platform {
	public:
		platform(const std::vector<std::string> &args);
		~platform();
		void commit_scene(cpu::scene *scene) override;
		bool interprete(const std::string &command, std::istringstream &in) override;
		
		batch_rt *rt = nullptr;
		cpu::scene *scene = nullptr;
		wf::raydata *raydata = nullptr; // usually, we have the ray data with the tracers, the CPU code is inocnsistent in that way. should be fixed some time.
	};

	extern platform *pf;
}
