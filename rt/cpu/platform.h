#pragma once

namespace wf::cpu {

	class platform : public wf::platform {
	public:
		platform(const std::vector<std::string> &args);
		bool interprete(const std::string &command, std::istringstream &in) override;
		wf::raydata *raydata = nullptr; // usually, we have the ray data with the tracers, the CPU code is inocnsistent in that way. should be fixed some time.
	};

}
