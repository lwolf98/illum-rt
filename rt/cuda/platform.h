#pragma once

#include "base.h"

namespace wf::cuda {
	
#define PSD_IMPL(T,NAME)                                                          \
		global_memory_buffer<T> data;                                             \
		per_sample_data(glm::ivec2 dim) : data(NAME, dim.x*dim.y) {               \
			rc->call_at_resolution_change[this] = [this](int w, int h) {          \
				data.resize(w*h);                                                 \
			};                                                                    \
		}                                                                         \
		~per_sample_data() {}

	template<typename T> struct per_sample_data : public wf::per_sample_data<T> {
		PSD_IMPL(T, "templated buffer")
	};
	template<> struct per_sample_data<vec3> : public wf::per_sample_data<vec3> {
		PSD_IMPL(float3, "float3 buffer")
	};
#undef PSD_IMPL

	class platform : public wf::platform {
	public:
		int warp_size;
		int multi_processor_count;
		
		platform(const std::vector<std::string> &args);
		~platform();
		void commit_scene(::scene *scene) override;
		bool interprete(const std::string &command, std::istringstream &in) override;
		
		raydata* allocate_raydata() override;
		per_sample_data<float>* allocate_float_per_sample() override;
		per_sample_data<vec3>*  allocate_vec3_per_sample()  override;

		scenedata *sd = nullptr;
		batch_rt *rt = nullptr;
	};

	extern platform *pf;
}
