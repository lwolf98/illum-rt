#pragma once

#include "rt.h"

#include <glm/glm.hpp>

struct camera {
	int w, h; //!< in pixel
	float fovy, aspect;
	glm::vec3 dir, pos, up;
	float near_w, near_h; //!< world-space near plane dimension (half of it, (0,0)-(w,h))
	camera(const glm::vec3 &dir, const glm::vec3 &pos, const glm::vec3 &up, float fovy, int w, int h)
	      : w(w), h(h),
	        fovy(fovy), aspect(float(w)/h),
	        dir(dir), pos(pos), up(up) {
		update_frustum(fovy, w, h);
	}
	void update_frustum(float fovy, int w, int h) {
		this->fovy = fovy;
		this->aspect = float(w)/h;
		this->w = w; this->h = h;
		near_h = tanf(float(M_PI) * fovy * 0.5f / 180.0f);
		near_w = aspect * near_h;
	}
};


//! Set up a camera ray. Expects the vertices handed in to be normalized
ray cam_ray(const camera &cam, int x, int y, glm::vec2 offset = glm::vec2(0,0));

//! Call and import obj file in blender to visualize the camera rays
void test_camrays(const camera &camera, int stride = 10);

