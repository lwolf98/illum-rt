#pragma once

#include "base.h"

namespace wf::cuda {
	
	template<typename T> struct per_sample_data : public wf::per_sample_data<T> {
		global_memory_buffer<T> data;
		per_sample_data(glm::ivec2 dim) : data("float buffer", dim.x*dim.y) {
			rc->call_at_resolution_change[this] = [this](int w, int h) {
				data.resize(w*h);
			};
		}
		~per_sample_data() {}
	};

	class platform : public wf::platform {
	public:
		int warp_size;
		int multi_processor_count;
		
		platform(const std::vector<std::string> &args);
		~platform();
		void commit_scene(::scene *scene) override;
		bool interprete(const std::string &command, std::istringstream &in) override;
		
		raydata* allocate_raydata() override;
		per_sample_data<float>* allocate_float_per_sample();

		scenedata *sd = nullptr;
		batch_rt *rt = nullptr;
	};

	extern platform *pf;
}
