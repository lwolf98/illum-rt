#include "rt.h"
#include "camera.h"
#include "scene.h"
#include "intersect.h"

#include "cmdline.h"

#include <png++/png.hpp>
#include <iostream>
#include <cstdio>
#include <omp.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/random.hpp>

using namespace std;
using namespace glm;
using namespace png;

rgb_pixel to_png(vec3 col01) {
	col01 = clamp(col01, vec3(0), vec3(1));
	col01 = pow(col01, vec3(1.0f/2.2f));
	return rgb_pixel(col01.x*255, col01.y*255, col01.z*255);
}

// rgb_pixel trace_pixel(int x, int y, const camera &camera, const scene &scene, const shader &shader) {
// 	rgb_pixel col(40,40,40);
// 
// 	ray ray = cam_ray(camera, x, y);
// 	triangle_intersection closest, intersection;
// 	for (int i = 0; i < scene.triangles.size(); ++i)
// 		if (intersect(scene.triangles[i], scene.vertices.data(), ray, intersection))
// 			if (intersection.t < closest.t) {
// 				closest = intersection;
// 				closest.ref = i;
// 			}
// 
// 	if (closest.t < FLT_MAX) {
// 		vec3 x = ray.o + closest.t * ray.d;
// 		const triangle &tri = scene.triangles[closest.ref];
// 		vec3 n = (1-closest.beta-closest.gamma)*scene.vertices[tri.a].norm
// 		       + closest.beta *scene.vertices[tri.b].norm
// 			   + closest.gamma*scene.vertices[tri.c].norm;
// 		const material &material = scene.materials[tri.material_id];
// 		col = to_png(shader(x, n, material));
// 	}
// 
// 	return col;
// }
// 

void repl(FILE *, scene &);

int main(int argc, char **argv)
{
	parse_cmdline(argc, argv);

// 	cout << to_string(cmdline.cam_pos) << endl;
// 	cout << to_string(cmdline.view_dir) << endl;
// 	cout << to_string(cmdline.world_up) << endl;

	camera camera(cmdline.view_dir, cmdline.cam_pos, cmdline.world_up,
				  cmdline.fovy, cmdline.vp_w, cmdline.vp_h);

	test_camrays(camera);

	image<rgb_pixel> out(cmdline.vp_w, cmdline.vp_h);
	//scene scene = load_scene(cmdline.scene);

	scene scene;
	repl(stdin, scene);

// 	out.write(cmdline.outfile);
	return 0;
}
