#pragma once

#include "wavefront.h"

#include "libgi/wavefront-rt.h"

class scene;

namespace wf::cpu {

	/*! The CPU platform.
	 *
	 *  Todo:
	 *  Does not yet support scene views.  Implementing this is in ::scene ist not easily possible as the data is stored in
	 *  std::vector for which we cannot easily build views. I think it would probably be best to step up custom data storage with
	 *  owning/non-owning flags for the cpu data and initially put aliases to the vector data in a wf::cpu::scenedata structure.
	 *  If any of the data fields should be replaced those can then be owning copies and a mechanism similar to
	 *  wf::cuda::global_memory_buffer can be implemented.
	 *
	 */
	class platform : public wf::platform {
	public:
		platform(const std::vector<std::string> &args);
		~platform();
		void commit_scene(cpu::scene *scene) override;
		bool interprete(const std::string &command, std::istringstream &in) override;
		
		raydata* allocate_raydata() override;
		
		batch_rt *rt = nullptr;
		cpu::scene *sd = nullptr;
// 		cpu::raydata *raydata = nullptr; // usually, we have the ray data with the tracers, the CPU code is inocnsistent in that way. should be fixed some time.
	};

	extern platform *pf;
}
