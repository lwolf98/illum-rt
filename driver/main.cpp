#include "libgi/rt.h"
#include "libgi/camera.h"
#include "libgi/scene.h"
#include "libgi/intersect.h"
#include "libgi/framebuffer.h"
#include "libgi/context.h"
#include "libgi/timer.h"

#include "interaction.h"

#include "cmdline.h"

#include <png++/png.hpp>
#include <iostream>
#include <chrono>
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

//! A hacky way to convert ms to a human readable indication of how long this is going to be.
std::string timediff(unsigned ms) {
	if (ms > 2000) {
		ms /= 1000;
		if (ms > 60) {
			ms /= 60;
			if (ms > 60) {
				return "hours";
			}
			else return std::to_string((int)floor(ms)) + " min";
		}
		else {
			return std::to_string((int)ms) + " sec";
		}
	}
	else return std::to_string(ms) + " ms";
}

/*! \brief This is called from the \ref repl to compute a single image
 *  
 *  Note: We first compute a single sample to get a rough estimate of how long rendering is going to take.
 *
 *  Note: Times reported via \ref stats_timer might not be perfectly reliable as of now.  This is because the
 *        timer-overhead is in the one (at times even two) digit percentages of the individual fragments measured.
 *
 */
void run(render_context &rc, gi_algorithm *algo) {
	using namespace std::chrono;
	algo->prepare_frame(rc);
	test_camrays(rc.scene.camera);
	rc.framebuffer.clear();

	auto start = system_clock::now();
	rc.framebuffer.color.for_each([&](unsigned x, unsigned y) {
										rc.framebuffer.add(x, y, algo->sample_pixel(x, y, 1, rc));
    								});
	auto delta_ms = duration_cast<milliseconds>(system_clock::now() - start).count();
	cout << "Will take around " << timediff(delta_ms*(rc.sppx-1)) << " to complete" << endl;
	
	rc.framebuffer.color.for_each([&](unsigned x, unsigned y) {
										rc.framebuffer.add(x, y, algo->sample_pixel(x, y, rc.sppx-1, rc));
    								});
	delta_ms = duration_cast<milliseconds>(system_clock::now() - start).count();
	cout << "Took " << timediff(delta_ms) << " (" << delta_ms << " ms) " << " to complete" << endl;
	
	algo->finalize_frame();
	
	rc.framebuffer.png().write(cmdline.outfile);
}

void rt_bench(render_context &rc) {
	//create Buffer for rays and intersections with the size of the camera resolution
	buffer<triangle_intersection> triangle_intersections(rc.scene.camera.w, rc.scene.camera.h);
	buffer<ray> rays(rc.scene.camera.w, rc.scene.camera.h);
	
	//init Buffer with Camera rays
	rays.for_each([&](unsigned x, unsigned y) {
		rays(x, y) = cam_ray(rc.scene.camera, x, y);
	});
	
	//calculate closest triangle intersection for each ray
	raii_timer bench_timer("rt_bench");
	rays.for_each([&](unsigned x, unsigned y) {
		triangle_intersections(x, y) = rc.scene.rt->closest_hit(rays(x, y));
	});
}

int main(int argc, char **argv)
{
	parse_cmdline(argc, argv);

	render_context rc;
	repl_update_checks uc;
	if (cmdline.script != "") {
		ifstream script(cmdline.script);
		repl(script, rc, uc);
	}
	if (cmdline.interact)
		repl(cin, rc, uc);

	stats_timer.print();

	delete rc.algo;
	return 0;
}
