#pragma once

#include "wavefront.h"

#include "libgi/wavefront-rt.h"

class scene;

namespace wf::cpu {

	template<typename T> struct per_sample_data : public wf::per_sample_data<T> {
		T *data = nullptr;
		per_sample_data(glm::ivec2 dim) : data(new T[dim.x * dim.y]) {
			rc->call_at_resolution_change[this] = [this](int w, int h) {
				delete [] data;
				data = new T[w * h];
			};
		}
		~per_sample_data() { delete [] data; }
	};

	/*! The CPU platform.
	 *
	 *  Todo:
	 *  Does not yet support scene views.  Implementing this is in ::scene ist not easily possible as the data is stored in
	 *  std::vector for which we cannot easily build views. I think it would probably be best to step up custom data storage with
	 *  owning/non-owning flags for the cpu data and initially put aliases to the vector data in a wf::cpu::scenedata structure.
	 *  If any of the data fields should be replaced those can then be owning copies and a mechanism similar to
	 *  wf::cuda::global_memory_buffer can be implemented.
	 *
	 */
	class platform : public wf::platform {
	public:
		platform(const std::vector<std::string> &args);
		~platform();
		void commit_scene(cpu::scene *scene) override;
		bool interprete(const std::string &command, std::istringstream &in) override;
		
		raydata* allocate_raydata() override;
		per_sample_data<float>* allocate_float_per_sample() override;
		per_sample_data<vec3>*  allocate_vec3_per_sample() override;
		
		batch_rt *rt = nullptr;
		cpu::scene *sd = nullptr;
	};

	extern platform *pf;
}
