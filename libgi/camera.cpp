#include "camera.h"

#include "random.h"

#include <fstream>

using namespace glm;
using namespace std;

//! Set up a camera ray using cam.up, cam.dir, cam.w, cam.h (see \ref camera::update_frustum)
ray cam_ray(const camera &cam, int x, int y, vec2 offset) {
#ifndef RTGI_SKIP_CAMRAY_SETUP_IMPL
	// find basis
	vec3 U = normalize(cross(cam.dir, cam.up));
	vec3 V = normalize(cross(U, cam.dir));
	// pixel position on near plane
	float u = ((float)x+0.5f+offset.x)/(float)cam.w * 2.0f - 1.0f;	// \in (-1,1)
	float v = ((float)y+0.5f+offset.y)/(float)cam.h * 2.0f - 1.0f;
	u = cam.near_w * u;	// \in (-near_w,near_w)
	v = cam.near_h * v;
	// near is implicitly at 1 (as per tanf above)
	return ray(cam.pos, normalize(cam.dir + U*u + V*v));
#else
	// todo: compute the camera rays for each pixel.
	// note: you can use the following function to export them as an obj model and look at them in blender.
	return ray(cam.pos, vec3(0,0,-1));
#endif
}

void test_camrays(const camera &camera, int stride) {
	ofstream out("test.obj");
	int i = 1;
	for (int y = 0; y < camera.h; y += stride)
		for (int x = 0; x < camera.w; x += stride) {
			ray ray = cam_ray(camera, x, y);
			out << "v " << ray.o.x << " " << ray.o.y << " " << ray.o.z << endl;
			out << "v " << ray.d.x << " " << ray.d.y << " " << ray.d.z << endl;
			out << "l " << i++ << " " << i++ << endl;
		}
}


