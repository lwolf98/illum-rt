#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <vector>
#include <string>

namespace gl {

	/*!	\brief Basic OpenGL Buffer abstraction (\see ssbo).
	 *
	 * 	Most likely for internal/extension use.
	 */
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


	/*! \brief Chunk of memory on the GPU available to OpenGL Shaders.
	 *
	 * 	The upload(onto the grpu)/download(from the gpu to ram) logic might be subject to change as this
	 * 	implementation can be wasteful in terms of host ram.
	 *
	 * 	Buffers are bound to pre-determined indices, but in some cases the buffer backing a specific index
	 * 	(e.g. ray data) may change (e.g. shadow vs path rays). To make sure the proper buffer is bound,
	 * 	call `bind()'.
	 */
	template<typename T> struct ssbo : public buffer
	{
		std::vector<T> org_data;

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
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (GLuint)index, id);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		void bind() {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, (GLuint)index, id);
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
		void download() {
			if (org_data.size() == 0)
				org_data.resize(size);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
			void *p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			memcpy(org_data.data(), (T*)p, sizeof(T)*size);
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		}
	};

}
