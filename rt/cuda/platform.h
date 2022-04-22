#pragma once

#include "libgi/wavefront-rt.h"

namespace wf::cuda {

	class platform : public wf::platform {
	public:
		int warp_size;
		int multi_processor_count;
		platform(const std::vector<std::string> &args);
		~platform();
		bool interprete(const std::string &command, std::istringstream &in) override;
	};

}
