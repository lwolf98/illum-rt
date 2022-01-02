#pragma once

#define WITH_SYNC_GL_STATS

#include "libgi/timer.h"
#include "libgi/wavefront-rt.h"

#include <GL/glew.h>
#include <string>
#include <iostream>

namespace wf {
	namespace gl {


		/*! \brief Take time of otherwise asynchronously running GL calls.
		 *
		 *  Note: this sequentializes GL tasks and may hide race conditions
		 */
		struct timer : public ::timer {
			std::map<std::string, std::pair<GLuint,GLuint>> queries;
			timer();
			void start(const std::string &name) override;
			void stop(const std::string &name) override;
		};
		extern timer gpu_timer;
		#ifdef WITH_SYNC_GL_STATS
		#define time_this_block_gpu(name) raii_timer<wf::gl::timer> raii_timer__##name(#name, gpu_timer)
		#else
		#define time_this_block_gpu(name)
		#endif
	

		/*!	\brief Basic OpenGL Buffer abstraction (\see ssbo).
		 *
		 * 	Moste likely for internal/extension use.
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
		 */
		template<typename T> class ssbo : public buffer
		{
		public:
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
			void download() {
				if (org_data.size() == 0)
					org_data.resize(size);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
				void *p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
				memcpy(org_data.data(), (T*)p, sizeof(T)*size);
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			}
		};

		
		/*! \brief Ray data, including intersection points
		 *
		 *  The indices given for the Shader Storage Buffer Objects (ssbo) correspond to the binding locations in the
		 *  shaders (\see compute_shader).
		 */
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

		
		/*! \brief A copy of the scene data kept on the GPU.
		 *
		 * 	At any changes to the scene, make sure to update this structure.
		 */
		struct scenedata {
			ssbo<vec4> vertex_pos, vertex_norm;
			ssbo<vec2> vertex_tc;
			ssbo<ivec4> triangles;
			scenedata()
			: vertex_pos("vertex_pos", 4, 0),
			  vertex_norm("vertex_norm", 5, 0),
		 	  vertex_tc("vertex_tc", 6, 0),
			  triangles("triangles", 7, 0) {
			}
			void upload(scene *scene);
		};

		
		/*! \brief Default OpenGL Ray Tracer.
		 *
		 * 	We might build different variants that derive from this, but note that if a new OpenGL ray tracer uses
		 * 	different data structures it will be best to have that ray tracer be a sibling, not a child, of this
		 * 	interface (because this interface holds the ray- and scene data, which will be different in this case).
		 */
		struct batch_rt : public batch_ray_tracer {
			raydata rd;
			scenedata sd;
			batch_rt() : rd(rc->resolution()) {
			}
		};

		
		/*! \brief Computation nodes for managing Rays and Intersections, aka computing Bounces
		 *
		 */
		struct ray_and_intersection_processing : public wf::ray_and_intersection_processing {
		};


		/*! \brief OpenGL Platform for Ray Tracing
		 * 	
		 * 	A non-optimized GL implementation of RTGI's interface, primarily as proof of concept and documentation for
		 * 	more advanced GPU (or CPU/SIMD) driven implementations.
		 *
		 * 	Open issues:
		 * 	- Headless GL
		 * 	- Structs for triangle intersections
		 * 	- Materials on the GPU
		 *
		 */
		class platform : public wf::platform {
		public:
			platform();
			static std::string standard_preamble;
		};
	}
}
