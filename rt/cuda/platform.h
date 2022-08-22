#pragma once

#include "base.h"

namespace wf::cuda {

	class platform : public wf::platform {
	public:
		int warp_size;
		int multi_processor_count;
		
		platform(const std::vector<std::string> &args);
		~platform();
		void commit_scene(::scene *scene) override;
		bool interprete(const std::string &command, std::istringstream &in) override;
		
		raydata* allocate_raydata() override;

		scenedata *sd = nullptr;
		batch_rt *rt = nullptr;
	};

	extern platform *pf;
}
