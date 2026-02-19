#include "libgi/rt.h"
#include "libgi/camera.h"
#include "libgi/scene.h"
#include "libgi/intersect.h"
#include "libgi/framebuffer.h"
#include "libgi/context.h"
#include "libgi/timer.h"
#include "libgi/denoise.h"

#include "libgi/global-context.h"

#include "interaction.h"

#include "cmdline.h"

#include "config.h"

#include <regex>

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
#include <sstream>

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
		queue_command("run", remove_prev_same_commands);
	else {
		if (cmdline.verbose)
			std::cout << "INFO: Frame finished" << std::endl;
		
		preview_finalized = true;
		algo->finalize_frame();
		if (rc->enable_denoising) {
			if (cmdline.verbose)
				std::cout << "INFO: Frame denoised" << std::endl;
			denoise(rc->framebuffer.color.w, rc->framebuffer.color.h, rc->framebuffer.color.data, rc->albedo_valid ? rc->framebuffer_albedo.color.data : nullptr, rc->normal_valid ? rc->framebuffer_normal.color.data : nullptr, true);
			if (preview_window) {
				preview_framebuffer->resize(rc->resolution().x * rc->resolution().y, rc->framebuffer.color.data);
				glFinish();
			}
		}
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
#ifdef RTGI_CONFIG_A1
	test_camrays(rc->scene.camera);
#endif
	rc->framebuffer.clear();
	rc->framebuffer_albedo.clear();
	rc->framebuffer_normal.clear();

	algo->compute_samples();
	algo->finalize_frame();

	if (rc->enable_denoising) {
		auto begin = std::chrono::high_resolution_clock::now();
		denoise(rc->framebuffer.color.w, rc->framebuffer.color.h, rc->framebuffer.color.data, rc->albedo_valid ? rc->framebuffer_albedo.color.data : nullptr, rc->normal_valid ? rc->framebuffer_normal.color.data : nullptr, true);
		auto end = std::chrono::high_resolution_clock::now();
		std::cout << "Denoising took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms" << std::endl;
	}

	/*string out_name = cmdline.outfile;
	//if (rc->vpl_count >= 0) {
	//	out_name = std::regex_replace(out_name, std::regex(".png"), "_v" + std::to_string(rc->vpl_count) + ".png");

	//std::string append = "s" + std::to_string
	const load_config &cfg = rc->last_config;
	static constexpr uint32_t approx_var = APPROXIMATION_VARIANT;
	static constexpr uint32_t comp_var = COMPRESSION_VARIANT;
	std::stringstream str;
	str << "_s" << cfg.subd_level;
	str << "_a" << cfg.bvh_align_level;
	if (cfg.subd_type_patches) {
		str << "_A" << approx_var;
		str << "_C" << comp_var;
	}
	if (cfg.displacement_strength != 0) {
		str << "_DA" << static_cast<int32_t>(cfg.displace_action);
		str << "_DS" << cfg.displacement_strength;
	}
	out_name = std::regex_replace(out_name, std::regex(".png"), str.str() + ".png");*/

	string out_name = rc->outfile_full(std::nullopt);
	std::cout << std::endl << out_name << std::endl << std::endl;
	rc->framebuffer.png().write(out_name);
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
