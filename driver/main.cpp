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
	cout << "Took " << timediff(delta_ms) << " to complete" << endl;
	
	rc.framebuffer.png().write("out.png");
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
