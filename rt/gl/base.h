#pragma once

#include "libgi/wavefront-rt.h"

#include <GL/glew.h>
#include <string>
#include <iostream>

namespace wf {
	namespace gl {

		struct buffer {
			GLuint id;
			std::string name;
			GLuint index;
			unsigned size;

			buffer(std::string name, GLuint index, unsigned size)
			: id(0), name(name), index(index), size(size) {
				glGenBuffers(1, &id);
			}
			virtual ~buffer() {
				glDeleteBuffers(1, &id);
			}
			virtual void print() {
			}
		};

		template<typename T> class ssbo : public buffer
		{
			std::vector<T> org_data;
		public:
			ssbo(std::string name, GLuint index, unsigned size)
			: buffer(name, index, size) {
				if (size > 0) resize(size);
			}

			ssbo(std::string name, GLuint index, const std::vector<T> &data)
			: buffer(name, index, data.size()), org_data(data) {
				if (size > 0) resize(size, data);
			}

			void resize(int size) {
				this->size = size;
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
				glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(T) * size, nullptr, GL_STATIC_READ);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (GLuint) index, id);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
			
			void resize(const std::vector<T> &data) {
				resize(data.size(), data.data());
			}
			void resize(int size, const T *data) {
				this->size = size;
				org_data.resize(size);
				std::copy(data, data + size, org_data.begin());
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
				glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(T) * size, data, GL_STATIC_READ);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (GLuint)index, id);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
		};

		struct raydata {
			int w, h;
			ssbo<vec4> rays_o;
			ssbo<vec4> rays_d;
			ssbo<vec4> rays_id;
			ssbo<vec4> intersections;

			raydata(glm::ivec2 dim) : raydata(dim.x, dim.y) {
			}
			raydata(int w, int h)
			: w(w), h(h),
			  rays_o("rays_o", 0, w*h),
			  rays_d("rays_d", 1, w*h),
			  rays_id("rays_id", 2, w*h),
			  intersections("intersections", 3, w*h) {
				  rc->call_at_resolution_change[this] = [this](int w, int h) {
					  this->w = w;
					  this->h = h;
					  rays_o.resize(w*h);
					  rays_d.resize(w*h);
					  rays_id.resize(w*h);
					  intersections.resize(w*h);
				  };
			}
		};

		struct batch_rt : public batch_ray_tracer {
			raydata rd;
			batch_rt() : rd(rc->resolution()) {
			}
		};

		class platform : public wf::platform {
		public:
			platform();
		};
	}
}
