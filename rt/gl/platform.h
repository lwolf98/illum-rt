#pragma once

#include "libgi/wavefront-rt.h"

#include "base.h"

namespace wf::gl {
	template<typename T> struct per_sample_data : public wf::per_sample_data<T> {
		data_texture<T> data;
		per_sample_data(glm::ivec2 dim) : data("float buffer", dim.x, dim.y, gl_internal_format<T>()) {
			rc->call_at_resolution_change[this] = [this](int w, int h) {
				data.resize(w, h);
			};
		}
		~per_sample_data() {}
	};
	template<> struct per_sample_data<vec3> : public wf::per_sample_data<vec3> {
		data_texture<vec4> data;
		per_sample_data(glm::ivec2 dim) : data("float buffer", dim.x, dim.y, GL_RGBA32F) {
			rc->call_at_resolution_change[this] = [this](int w, int h) {
				data.resize(w, h);
			};
		}
		~per_sample_data() {}
	};

	
	/*! \brief OpenGL Platform for Ray Tracing
	 * 	
	 * 	A non-optimized GL implementation of RTGI's interface, primarily as proof of concept and documentation for
	 * 	more advanced GPU (or CPU/SIMD) driven implementations. Well, and then it got a little out of hand. However,
	 * 	for a very simple use case see ../cpu/, and consider this wavefront implementation a (at least comparatively)
	 * 	approachable implementation of a more complex use case.
	 *
	 *  Does not support scene views. Implementing them can be done similarly to wf::cuda::global_memory_buffer and
	 *  wf::cuda::scenedata.
	 */
	class platform : public wf::platform {
	public:
		platform(const std::vector<std::string> &args);
		~platform();
		void commit_scene(::scene *scene);
		bool interprete(const std::string &command, std::istringstream &in) override;
		
		raydata* allocate_raydata() override;
		per_sample_data<float>* allocate_float_per_sample() override;
		per_sample_data<vec3>*  allocate_vec3_per_sample() override;
		
		scenedata *sd = nullptr;
		batch_rt *rt = nullptr;
	};
	
	extern platform *pf;
}
