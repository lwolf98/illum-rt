#include "framebuffer.h"

using namespace glm;


void framebuffer::clear() {
	color.clear(vec4(0,0,0,0));
}

void framebuffer::add(unsigned x, unsigned y, vec3 res) {
	auto &c = color(x,y);
	c = vec4(vec3(c) + res, c.w + 1);
}

png::image<png::rgb_pixel> framebuffer::png() const {
	png::image<png::rgb_pixel> out(color.w, color.h);
	
	color.for_each([&](unsigned x, unsigned y) {
					vec3 c(color(x,y));
					c = pow(clamp(c, vec3(0), vec3(1)), vec3(1.0f/2.2f)) * 255.0f;
				   	out[color.h-y-1][x] = png::rgb_pixel(c.x, c.y, c.z);
				   });
	return out;
}
