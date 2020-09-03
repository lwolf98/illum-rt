#pragma once

#include <glm/glm.hpp>

#include <utility>
#include <vector>
#include <string>
#include <sstream>

#include "rt.h"

class scene;
class render_context;

class gi_algorithm {
public:
	typedef std::vector<pair<vec3, vec2>> sample_result;

	virtual bool interprete(const std::string &command, std::istringstream &in) { return false; }
	virtual void prepare_frame(const render_context &rc) {}
	virtual sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &rc) = 0;
	virtual ~gi_algorithm(){}
};

