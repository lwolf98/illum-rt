#include "algorithm.h"

#include "libgi/rt.h"
#include "libgi/context.h"
#include "libgi/util.h"
#ifndef RTGI_SKIP_SIMPLE_PT
#include "libgi/sampling.h"
#endif

#include "libgi/global-context.h"
#include "config.h"

#ifdef HAVE_GL
#include "driver/preview.h"
#endif

#include <chrono>

float gi_algorithm::uniform_float() const {
	return rc->rng.uniform_float();
}

glm::vec2 gi_algorithm::uniform_float2() const {
	return rc->rng.uniform_float2();
}

#ifndef RTGI_SKIP_SIMPLE_PT
std::tuple<ray,float> gi_algorithm::sample_uniform_direction(const diff_geom &hit) const {
	// set up a ray in the hemisphere that is uniformly distributed
	vec2 xi = rc->rng.uniform_float2();
	float z = xi.x;
	float phi = 2*pi*xi.y;
	// z is cos(theta), sin(theta) = sqrt(1-cos(theta)^2)
	float sin_theta = sqrtf(1.0f - z*z);
	vec3 sampled_dir = vec3(sin_theta * cosf(phi),
							sin_theta * sinf(phi),
							z);
	
	vec3 w_i = align(sampled_dir, hit.ng);
	ray sample_ray(hit.x, w_i);
	return { sample_ray, one_over_2pi };
}

std::tuple<ray,float> gi_algorithm::sample_cosine_distributed_direction(const diff_geom &hit) const {
	vec2 xi = rc->rng.uniform_float2();
	vec3 sampled_dir = cosine_sample_hemisphere(xi);
	vec3 w_i = align(sampled_dir, hit.ng);
	ray sample_ray(hit.x, w_i);
	return { sample_ray, sampled_dir.z * one_over_pi };
}

std::tuple<ray,float> gi_algorithm::sample_brdf_distributed_direction(const diff_geom &hit, const ray &to_hit) const {
	auto [w_i, f, pdf, t] = hit.mat->brdf->sample(hit, -to_hit.d, rc->rng.uniform_float2());
	ray sample_ray(hit.x, w_i);
	return {sample_ray, pdf};
}
#endif


//! A hacky way to convert ms to a human readable indication of how long this is going to be.
static std::string timediff(unsigned ms) {
	if (ms > 2000) {
		ms /= 1000;
		if (ms > 60) {
			ms /= 60;
			if (ms > 121) return "hours";
			else return std::to_string((int)floor(ms+0.5)) + " min";
		}
		else return std::to_string((int)(ms+0.5)) + " sec";
	}
	else return std::to_string(ms) + " ms";
}
static std::string timediff_when(unsigned ms) {
	if (ms > 2000) {
		ms /= 1000;
		if (ms > 200) {
			ms /= 60;
			auto done = std::chrono::system_clock::now() + std::chrono::milliseconds(ms*60*1000);
			auto tt = std::chrono::system_clock::to_time_t(done);
			auto loc = std::localtime(&tt);
			char doneat[6];
			strftime(doneat, 6, "%H:%M", loc);
			return std::string(", check back at ") + doneat;
		}
	}
	return "";
}

/*  Implementation for "one path at a time" traversal on the CPU over the complete image.
 *
 *  Note: We measure the execution time of the first sample to get a rough estimate of how long rendering is going to take.
 *
 *  Note: Times reported via \ref stats_timer might not be perfectly reliable as of now.  This is because the
 *        timer-overhead is in the one (at times even two) digit percentages of the individual fragments measured.
 *
 */
void recursive_algorithm::compute_samples() {
	using namespace std::chrono;
	auto start = system_clock::now();
	for (current_sample_index = 0; current_sample_index < rc->sppx; current_sample_index++) {
		rc->framebuffer.color.for_each([&](unsigned x, unsigned y) {
										rc->framebuffer.add(x, y, sample_pixel(x, y));
									   });
		if (current_sample_index == 0) {
			auto delta_ms = duration_cast<milliseconds>(system_clock::now() - start).count();
			std::cout << "Will take around " << timediff(delta_ms*(rc->sppx-1)) << " to complete" << timediff_when(delta_ms*(rc->sppx-1)) << std::endl;	
		}
	}

	auto delta_ms = duration_cast<milliseconds>(system_clock::now() - start).count();
	std::cout << "Took " << timediff(delta_ms) << " (" << delta_ms << " ms) " << " to complete" << std::endl;

	rc->framebuffer.color.for_each([&](unsigned x, unsigned y) {
		glm::vec4 col = rc->framebuffer.color(x,y);
		rc->framebuffer.color(x, y) = col / col.w;
	});
}

bool recursive_algorithm::compute_sample() { 
	if (current_sample_index >= rc->sppx) return false;

	glm::ivec2 res = rc->resolution();

	glm::ivec2 render_res = res;
	render_res += rc->preview_offset - 1;
	render_res /= rc->preview_offset;

	#pragma omp parallel for
	for (int x = 0; x < render_res.x; x++)
		for (int y = 0; y < render_res.y; y++) {
			unsigned real_x = x * rc->preview_offset + current_preview_offset.x;
			unsigned real_y = y * rc->preview_offset + current_preview_offset.y;
			if (real_x < res.x && real_y < res.y)
				rc->framebuffer.add(real_x, real_y, sample_pixel(real_x, real_y));
		}
	
#ifdef HAVE_GL
	if (preview_window) {
		glfwMakeContextCurrent(render_window);

		auto res = rc->resolution();
		preview_framebuffer->resize(res.x * res.y, rc->framebuffer.color.data);
		glFinish();
	}
#endif
	next_preview_offset();
	return current_sample_index < rc->sppx;
}

void recursive_algorithm::next_preview_offset() {
	if(++current_preview_offset.x < rc->preview_offset)
		return;
	current_preview_offset.x = 0;
	if(++current_preview_offset.y < rc->preview_offset)
		return;
	current_preview_offset.y = 0;
	current_sample_index++;
}

void recursive_algorithm::prepare_frame() {
	rc->framebuffer.clear();
	rc->framebuffer_albedo.clear();
	rc->framebuffer_normal.clear();
	current_preview_offset = glm::ivec2(0);
	gi_algorithm::prepare_frame();
	assert(rc->scene.rt != nullptr);
}

void wavefront_algorithm::prepare_frame() {
	gi_algorithm::prepare_frame();
}
