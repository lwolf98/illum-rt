#pragma once

#include "scene.h"
#include "random.h"
#include "framebuffer.h"
#include "driver/cmdline.h"

#include <functional>
#include <map>
#include <regex>
#include <optional>
#include <chrono>
#include <ctime>
#include <iomanip>
//#include <format>
//#include "gi/manylight.h"
struct vpl;

struct gi_algorithm;

namespace wf {
	class platform;
}

/* \brief Stores contextual information the rendering functions make use of.
 *
 */
struct render_context {
	::rng_std_mt rng;
	::scene scene;
	::framebuffer framebuffer;
	gi_algorithm *algo = nullptr;
	unsigned int sppx = 1;
	unsigned int preview_offset = 1;
	wf::platform *platform = nullptr;

	/* global VPL data */
	std::vector<vpl> *vpls;
	bool ml_cpu_preparation = false;
	int vpl_count = -1;

	load_config last_config;

	bool enable_denoising = false;
	// Store albedo information for denoising. Values need to be in the range of [0, 1]. Don't forget to set albedo_valid if used.
	::framebuffer framebuffer_albedo;
	bool albedo_valid = false;
	// Store normal information for denoising. Values need to be in the range of [-1, 1]. The vectors can be of arbitrary length, world-space or view-space-aligned. Don't forget to set normal_valid if used.
	::framebuffer framebuffer_normal;
	bool normal_valid = false;

	render_context() : framebuffer(scene.camera.w, scene.camera.h), framebuffer_albedo(scene.camera.w, scene.camera.h), framebuffer_normal(scene.camera.w, scene.camera.h) {
		call_at_resolution_change[&framebuffer] = [this](int w, int h) { framebuffer.resize(w, h); };
		call_at_resolution_change[&scene] = [this](int w, int h) { scene.camera.update_frustum(scene.camera.fovy, w, h); };
		call_at_resolution_change[&framebuffer_albedo] = [this](int w, int h) { framebuffer_albedo.resize(w, h); };
		call_at_resolution_change[&framebuffer_normal] = [this](int w, int h) { framebuffer_normal.resize(w, h); };
	}

	glm::ivec2 resolution() const {
		return glm::ivec2(framebuffer.color.w, framebuffer.color.h);
	}
	int w() const { return framebuffer.color.w; }
	int h() const { return framebuffer.color.h; }

	std::string outfile_full(std::optional<std::string> opt_suffix, bool rm_prefix = false, bool add_timestamp = false) {
		using namespace std::chrono;

		std::string out_name = cmdline.outfile;
		const load_config &cfg = last_config;
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

		std::string type = ".png";
		if (opt_suffix)
			type = *opt_suffix;

		std::string timestamp = "";
		if (add_timestamp) {
			auto now = std::chrono::system_clock::now();
			std::time_t t = std::chrono::system_clock::to_time_t(now);
			std::tm tm{};
			localtime_r(&t, &tm);
			std::ostringstream oss;
			oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
			timestamp = "__" + oss.str();
			//timestamp = std::format("{:%Y-%m-%d_%H-%M-%S}", system_clock::now());
		}

		out_name = std::regex_replace(out_name, std::regex(".png"), str.str() + timestamp + type);
		if (rm_prefix)
			out_name = std::regex_replace(out_name, std::regex(R"(.*\/)"), "");
		return out_name;
	}

	std::map<void*, std::function<void(int,int)>> call_at_resolution_change;
	void change_resolution(int w, int h) {
		assert(w > 0 && h > 0);
		for (auto [_,f] : call_at_resolution_change)
			f(w, h);
	}
};

