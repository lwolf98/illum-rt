#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"
#include "libgi/scene.h"
#include "gi/direct.h"
#include <vector>

#include "libgi/global-context.h"

struct vpl;
void throughput_stats(const vec3 tp[], const int start, const int size);
void vpl_stats(const std::vector<vpl>& vpls);
void vpl_stats(const vpl* vpls, const int size);
void framebuffer_stats();

// virtual point light
struct vpl : public pointlight {
//struct vpl : public pointlight {
	triangle_intersection is;
	vec3 normal; //optional with 'is'
	vec3 w_in;

	// Constructor for VPLs (v_1 to v_...)
	vpl(const vec3& col, const vec3& pos, const vec3& normal, const vec3& w_in, const triangle_intersection& is)
	: pointlight(pos, col), normal(normal), w_in(w_in), is(is) {}

	//vpl() : pointlight(vec3(0), vec3(0)) {}
	vpl() : vpl(vec3(0), vec3(0), vec3(0), vec3(0), triangle_intersection()) {}
};

class manylight_algorithm : public direct_light {
private:
	enum sampling_mode sampling_mode = sample_light;

	uint32_t paths;
	uint32_t path_length;
	uint32_t vpl_integrations = 1;
	std::vector<vpl> vpls;

public:
	manylight_algorithm(uint32_t paths, uint32_t path_length) : paths(paths), path_length(path_length) {}

	/* Generate VPLs */
	void prepare_frame() override;

	/* Integration via the previously generated VPLs */
	vec3 sample_pixel(uint32_t x, uint32_t y) override;
};

#ifndef RTGI_SKIP_WF
#include "manylight-steps.h"
namespace wf {
	//TODO-ML: check if inheritance is correctly setup (fields, visibility, virtual, override)
	class manylight_algorithm : public direct_light {
		uint32_t paths = 32;
		uint32_t path_length = 10;
		uint32_t vpls_per_sample = 6;
		uint32_t rr_start = 4;

		//std::vector<vpl>* vpls = nullptr;
		//TODO-ML: maybe remove vpls and only use vpl_store (reorganize it in the copy step)
		vpldata* vpls = nullptr;
		per_sample_data<int>* vpl_count = nullptr;

		vpldata* vpl_store = nullptr;
		per_sample_data<int>* vpl_store_offset = nullptr;
		raydata* vpl_rays = nullptr;
		per_sample_data<vec3>* light_throughput = nullptr;
		per_sample_data<vec3>* le = nullptr;
		vpldata* sampled_vpls = nullptr;

		void regenerate_steps() override;
		/*vpl* allocate_vpl_store();
		vec3* allocate_light_throughput();
		std::vector<vpl>* allocate_vpls();
		vpl* allocate_vpl_per_sample();*/

	public:
		uint32_t current_depth = 0;

		manylight_algorithm();
		bool interprete(const std::string &command, std::istringstream &in) override;
		uint32_t get_paths() const {
			return paths;
		}
		uint32_t get_path_length() const {
			return path_length;
		}
		uint32_t get_rr_start() const {
			return rr_start;
		}
		uint32_t get_vpls_per_sample() const {
			return vpls_per_sample;
		}
		uint32_t get_vpl_count() const {
			if (vpls)
				return vpls->size();

			return 0;
		}
	};
}
#endif