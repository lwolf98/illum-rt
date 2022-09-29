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

#include "config.h"

#ifdef HAVE_GL
#include "preview.h"
#endif

#include "gi/primary-hit.h"

#include <png++/png.hpp>
#include <iostream>
#include <chrono>
#include <cstdio>
#include <omp.h>
#include <thread>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/random.hpp>
#include <glm/glm.hpp>

#include "config.h"

using namespace std;
using namespace glm;
using namespace png;

rgb_pixel to_png(vec3 col01) {
	col01 = clamp(col01, vec3(0), vec3(1));
	col01 = pow(col01, vec3(1.0f/2.2f));
	return rgb_pixel(col01.x*255, col01.y*255, col01.z*255);
}

#ifdef HAVE_GL
void run_sample(gi_algorithm *algo) {
	if (!algo) return;
	std::chrono::time_point<std::chrono::high_resolution_clock> start = chrono::high_resolution_clock::now();

	if (preview_update_in_progress) {
		preview_update_in_progress = false;
		preview_finalized = false;
		algo->prepare_frame();
		algo->compute_sample();
		queue_command("run", remove_prev_same_commands);
	}
	else if (algo->compute_sample())
		queue_command("run");
	else {
		if (cmdline.verbose)
			std::cout << "INFO: Frame finished" << std::endl;
		
		preview_finalized = true;
		algo->finalize_frame();
	}

	auto end = chrono::high_resolution_clock::now();
	delta_time = chrono::duration<double, milli>(end-start).count();
	start = end;
}
#endif

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

void start_repl_and_process_commands() {
	thread repls(run_repls);
	process_command_queue();
	repls.join();
}

/*! \brief When we render without a preview we start a thread for the repl and process the commands on the main thread
 *  If the preview is active the preview render loop is processed on the main thead.
 *  This is done as some GL calls depend on being called from the main thread.
 *  The processing of commands is done on a seperate thread to ensure a responsive preview.
 */
int main(int argc, char **argv)
{
	parse_cmdline(argc, argv);

#ifdef HAVE_GL
	if (preview_window) {
		thread repls(run_repls);
		thread process_commands(process_command_queue);
		
		render_preview();
		
		process_commands.join();
		repls.join();
		terminate_gl();
	}
	else start_repl_and_process_commands();
#else
	start_repl_and_process_commands();
#endif

	stats_timer.print();

	delete rc->algo;
	return 0;
}
