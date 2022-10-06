#pragma once

#include <glm/glm.hpp>

#include <utility>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

#include "rt.h"

class scene;
class render_context;

/*  \brief All rendering algorithms should rely on this interface.
 *
 *  The interpret function is called for repl-commands that are not handled by
 *  \ref repl directly. Return true if your algorithm accepted the command.
 *
 *  prepare_frame can be used to initialize things before a number of samples
 *  are rendered.
 *
 *  Note: This is still a little messy in that a few things should be moved out
 *  (see comment below) and that a few things (also from inside render_context)
 *  should go into recursive_algorithm (or cpu_rendering_context if we get to
 *  that).
 *
 */
class gi_algorithm {
protected:
	float uniform_float() const;
	glm::vec2 uniform_float2() const;
#ifndef RTGI_SKIP_SIMPLE_PT
	// maybe these should go into a seperate with_importance_sampling mixin...
	// or be free standing functions, is there any need for those to be grouped in here?
	std::tuple<ray,float> sample_uniform_direction(const diff_geom &hit) const;
	std::tuple<ray,float> sample_cosine_distributed_direction(const diff_geom &hit) const;
	std::tuple<ray,float> sample_brdf_distributed_direction(const diff_geom &hit, const ray &to_hit) const;
#endif

public:
	bool data_reset_required = true;
	unsigned current_sample_index = 0;
	
	virtual bool interprete(const std::string &command, std::istringstream &in) { return false; }
	virtual void prepare_data() {}
	virtual void prepare_frame() {
		current_sample_index = 0;
		if (data_reset_required)
			data_reset_required = false, prepare_data();
	}
	virtual void finalize_frame() {}
	virtual void compute_samples() = 0;
	virtual bool compute_sample() = 0;
	virtual ~gi_algorithm(){}
};
 


/*  This is the basic CPU style "one path at a time, all the way down" algorithm.
 *
 *  sample_pixel is called for each pixel in the target-image to compute a number of samples which are accumulated by
 *  the \ref framebuffer.
 *   - x, y are the pixel coordinates to sample a ray for.
 *   - samples is the number of samples to take
 *   - render_context holds contextual information for rendering (e.g. a random number generator)
 *
 */
class recursive_algorithm : public gi_algorithm {
public:
	using gi_algorithm::gi_algorithm;
	glm::ivec2 current_preview_offset = glm::ivec2(0);

	void next_preview_offset();
	virtual glm::vec3 sample_pixel(uint32_t x, uint32_t y) = 0;

	void compute_samples() override;
	bool compute_sample() override;
	void prepare_frame() override;
};

/*  This is the basic GPU-style "one segment at a time" algorithm.
 *  
 *  Use this to provide algorithms of this kind.
 *  
 */
class wavefront_algorithm : public gi_algorithm {
	using gi_algorithm::gi_algorithm;
public:
	void prepare_frame() override;
};
