#pragma once

namespace wf::cpu {

	class platform : public wf::platform {
	public:
		platform(const std::vector<std::string> &args);
		bool interprete(const std::string &command, std::istringstream &in) override;
	};

}
