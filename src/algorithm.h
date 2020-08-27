#pragma once

#include <glm/glm.hpp>

#include <utility>
#include <vector>
#include <string>
#include <sstream>

class scene;

class gi_algorithm {
public:
	typedef std::vector<std::pair<glm::vec3, glm::vec2>> sample_result;

	bool interprete(const std::string &command, std::istringstream &in) { return false; }
	virtual void prepare_frame(const scene &scene) {}
	virtual sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const scene &scene) = 0;
	virtual ~gi_algorithm(){}
};

