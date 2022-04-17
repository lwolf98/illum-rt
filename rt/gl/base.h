#pragma once

#define WITH_SYNC_GL_STATS

#include "libgi/timer.h"
#include "libgi/wavefront-rt.h"

#include "opengl.h"
#include "bindings.h"

#include <string>
#include <iostream>

namespace wf {
	namespace gl {

		struct platform;

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


		void enable_gl_debug_output();
		void disable_gl_debug_output();
		void enable_gl_notifications();
		void disable_gl_notifications();
	

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

		
		/*! \brief Ray data, including intersection points
		 *
		 *  The indices given for the Shader Storage Buffer Objects (ssbo) correspond to the binding locations in the
		 *  shaders (\see compute_shader).
		 *  
		 *  Note: If we add different ray layouts this might be called raydata_soa and raydata be the name of the
		 *  platform-specific base class referenced in \ref gl::platform. Invidividual ray tracers would then hold the
		 *  appropriate pointer to their ray data
		 */
		struct raydata : public wf::raydata {
			int w, h;
			ssbo<vec4> rays;
			ssbo<vec4> intersections;
			ssbo<vec4> framebuffer;

			raydata(glm::ivec2 dim) : raydata(dim.x, dim.y) {
			}
			raydata(int w, int h)
			: w(w), h(h),
			  rays("rays", BIND_RAYS, 3*w*h),
			  intersections("intersections", BIND_ISEC, w*h),
			  framebuffer("framebuffer", BIND_FBUF, w*h) {
				  rc->call_at_resolution_change[this] = [this](int w, int h) {
					  this->w = w;
					  this->h = h;
					  rays.resize(w*h*3);
					  intersections.resize(w*h);
					  framebuffer.resize(w*h);
				  };
			}
		};

		
		/*! \brief A copy of the scene data kept on the GPU.
		 *
		 * 	At any changes to the scene, make sure to update this structure.
		 */
		struct scenedata {
			struct vertex {
				vec4 pos, norm;
				vec2 tc;
				vec2 dummy;
			};
			struct material {
				vec4 albedo, emissive;
				GLuint64 albedo_tex;
				GLint has_tex;
			};
			ssbo<vertex> vertices;
			ssbo<ivec4> triangles;
			ssbo<material> materials;
			std::vector<GLuint> textures;
			scenedata()
			: vertices("vertices", BIND_VERT, 0),
			  triangles("triangles", BIND_TRIS, 0),
			  materials("materials", BIND_MTLS, 0) {
			}
			~scenedata();
			void upload(scene *scene);
		};

		
		/*! \brief Default OpenGL Ray Tracer.
		 *
		 */
		struct batch_rt : public batch_ray_tracer {
			gl::scenedata *sd = nullptr;
			gl::raydata *rd = nullptr;
			void build(::scene *scene) {
				rd = new raydata(rc->resolution());
				// make this a simple c'tor?
				sd = new scenedata;
				sd->upload(scene);
			}
		};

		
		/*! \brief Computation nodes for managing Rays and Intersections, aka computing Bounces
		 *
		 */
		struct ray_and_intersection_processing : public wf::ray_and_intersection_processing {
			batch_rt *rt;	// most common base class possible to have the proper ray and scene layout
			                // might have to be moved to derived classes
			void use(wf::batch_ray_tracer *that) override { rt = dynamic_cast<gl::batch_rt*>(that); }
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
		};
	}
}
