#pragma once

#include "scene.h"
#include "random.h"
#include "framebuffer.h"

#include <functional>
#include <map>

struct gi_algorithm;

namespace wf {
	class platform;
}

/* \brief Stores contextual information the rendering functions make use of.
 *
 */
struct render_context {
	::rng_std_mt rng;
	::scene scene;
	::framebuffer framebuffer;
	gi_algorithm *algo = nullptr;
	unsigned int sppx = 1;
	unsigned int preview_offset = 1;
	wf::platform *platform = nullptr;
	render_context() : framebuffer(scene.camera.w, scene.camera.h) {
		call_at_resolution_change[&framebuffer] = [this](int w, int h) { framebuffer.resize(w, h); };
		call_at_resolution_change[&scene] = [this](int w, int h) { scene.camera.update_frustum(scene.camera.fovy, w, h); };
	}

	glm::ivec2 resolution() const {
		return glm::ivec2(framebuffer.color.w, framebuffer.color.h);
	}
	int w() const { return framebuffer.color.w; }
	int h() const { return framebuffer.color.h; }

	std::map<void*, std::function<void(int,int)>> call_at_resolution_change;
	void change_resolution(int w, int h) {
		assert(w > 0 && h > 0);
		for (auto [_,f] : call_at_resolution_change)
			f(w, h);
	}
};

