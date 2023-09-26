#pragma once

#include "libgi/algorithm.h"
#include "libgi/material.h"

#ifndef RTGI_SKIP_WF
#include "libgi/wavefront-rt.h"
#endif


#ifndef RTGI_SKIP_DIRECT_ILLUM
class direct_light : public recursive_algorithm {
public:
	enum sampling_mode { sample_uniform, sample_cosine, sample_light, sample_brdf };
private:
	enum sampling_mode sampling_mode = sample_light;

protected:
	vec3 sample_uniformly(const diff_geom &hit, const ray &view_ray);
	vec3 sample_lights(const diff_geom &hit, const ray &view_ray);
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	vec3 sample_cosine_weighted(const diff_geom &hit, const ray &view_ray);
	vec3 sample_brdfs(const diff_geom &hit, const ray &view_ray);
#endif
	
#ifndef RTGI_AXX
	glm::vec3 full_mis(uint32_t x, uint32_t y);
#endif

public:
	glm::vec3 sample_pixel(uint32_t x, uint32_t y) override;
	bool interprete(const std::string &command, std::istringstream &in) override;
	void finalize_frame() override;
};

#ifndef RTGI_SKIP_DIRECT_MIS
class direct_light_mis : public direct_light {
public:
	glm::vec3 sample_pixel(uint32_t x, uint32_t y) override;
	bool interprete(const std::string &command, std::istringstream &in) override;
};
#endif
#endif

#ifndef RTGI_SKIP_WF
#include "gi/direct-steps.h"
namespace wf {
	class direct_light : public simple_algorithm {
		raydata *camrays = nullptr,
				*shadowrays = nullptr;
		per_sample_data<float> *pdf = nullptr;
		per_sample_data<vec3> *lightcol = nullptr;

		enum ::direct_light::sampling_mode sampling_mode = ::direct_light::sample_uniform;
		void regenerate_steps();
	public:
		direct_light();
		bool interprete(const std::string &command, std::istringstream &in) override;
	};
}
#endif

