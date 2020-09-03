#pragma once

#include "scene.h"
#include "random.h"
#include "framebuffer.h"

struct gi_algorithm;

struct render_context {
	::rng rng;
	::scene scene;
	::framebuffer framebuffer;
	gi_algorithm *algo = nullptr;
	unsigned int sppx = 1;
	render_context() : framebuffer(scene.camera.w, scene.camera.h) {}
};
