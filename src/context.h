#pragma once

#include "scene.h"
#include "random.h"
#include "framebuffer.h"

struct render_context {
	::rng rng;
	::scene scene;
	::framebuffer framebuffer;
	unsigned int sppx = 1;
	render_context() : framebuffer(scene.camera.w, scene.camera.h) {}
};
