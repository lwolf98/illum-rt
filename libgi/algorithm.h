#pragma once

#include <glm/glm.hpp>

#include <utility>
#include <vector>
#include <string>
#include <sstream>

#include "rt.h"

class scene;
class render_context;

/*  \brief All rendering algorithms should rely on this interface.
 *
 *  The interpret function is called for repl-commands that are not handled by \ref repl directly. Return true if your
 *  algorithm accepted the command.
 *
 *  prepare_frame can be used to initialize things before a number of samples are rendered.
 *
 *  sample_pixel is called for each pixel in the target-image to compute a number of samples which are accumulated by
 *  the \ref framebuffer.
 *   - x, y are the pixel coordinates to sample a ray for.
 *   - samples is the number of samples to take
 *   - render_context holds contextual information for rendering (e.g. a random number generator)
 *
 */
class gi_algorithm {
public:
	typedef std::vector<pair<vec3, vec2>> sample_result;

	virtual bool interprete(const std::string &command, std::istringstream &in) { return false; }
	virtual void prepare_frame(const render_context &rc) {}
	virtual sample_result sample_pixel(uint32_t x, uint32_t y, uint32_t samples, const render_context &rc) = 0;
	virtual ~gi_algorithm(){}
};

