#include "libgi/rt.h"
#include "libgi/camera.h"
#include "libgi/scene.h"
#include "libgi/intersect.h"
#include "libgi/framebuffer.h"
#include "libgi/context.h"
#include "libgi/timer.h"

#include "libgi/global-context.h"

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

/*! \brief This is called from the \ref repl to compute a single image
 *  
 */
void run(gi_algorithm *algo) {
	using namespace std::chrono;
	algo->prepare_frame();
	test_camrays(rc->scene.camera);
	rc->framebuffer.clear();

	algo->compute_samples();
	algo->finalize_frame();
	
	rc->framebuffer.png().write(cmdline.outfile);
}

int main(int argc, char **argv)
{
	parse_cmdline(argc, argv);

	repl_update_checks uc;
	if (cmdline.script != "") {
		ifstream script(cmdline.script);
		if(script.fail()){
			cerr << "Script file not found" << endl;
			return -1;
		}
		repl(script, uc);
	}
	if (cmdline.interact)
		repl(cin, uc);

	stats_timer.print();

	delete rc->algo;
	return 0;
}
