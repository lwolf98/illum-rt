#pragma once

#include "libgi/wavefront-rt.h"

namespace wf::cuda {

	class batch_rt;
	class scenedata;

	class platform : public wf::platform {
	public:
		int warp_size;
		int multi_processor_count;
		
		platform(const std::vector<std::string> &args);
		~platform();
		void commit_scene(::scene *scene) override;
		bool interprete(const std::string &command, std::istringstream &in) override;

		scenedata *sd = nullptr;
		batch_rt *rt = nullptr;
	};

	extern platform *pf;
}
