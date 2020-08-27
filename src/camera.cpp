#include "camera.h"

#include "random.h"

#include <fstream>

using namespace glm;
using namespace std;

ray cam_ray(const camera &cam, int x, int y, vec2 offset) {
	// find basis
	vec3 U = cross(cam.up, cam.dir);
	vec3 V = cross(cam.dir, U);
	// pixel position on near plane
	float u = ((float)(cam.w-x-1)+0.5f+offset.x)/(float)cam.w * 2.0f - 1.0f;	// \in (-1,1)
	float v = ((float)y+0.5f+offset.y)/(float)cam.h * 2.0f - 1.0f;
	u = cam.near_w * u;	// \in (-near_w,near_w)
	v = cam.near_h * v;
	// near is implicitly at 1 (as per tanf above)
	return ray(cam.pos, normalize(cam.dir + U*u + V*v));
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


