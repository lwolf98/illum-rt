#pragma once

#define WITH_SYNC_GL_STATS

#include "libgi/timer.h"
#include "libgi/wavefront-rt.h"
#include "libgi/random.h"

#include "opengl.h"
#include "bindings.h"

#include <string>
#include <iostream>
#include <type_traits>
#include <stdexcept>

namespace wf {
	namespace gl {

		struct platform;

		enum texture_support_mode_t { NO_TEX, PROPER_BINDLESS, HACKY };
		extern texture_support_mode_t texture_support_mode;

		//! \brief Take time of asynchronously running GL calls.
		struct timer : public wf::timer {
			std::map<std::string, std::pair<GLuint,GLuint>> queries;
			timer();
			void start(const std::string &name) override;
			void stop(const std::string &name) override;
			void synchronize() override;
		};

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

		template<typename T> struct data_texture {
			GLuint id = 0;
			int w = 0, h = 0;
			std::string name;
			GLenum internal_format;
			std::vector<T> org_data;

			data_texture(const std::string &name, int w, int h, GLenum internal_format) : name(name), internal_format(internal_format) {
				glGenTextures(1, &id);
				resize(w, h);
			}
			~data_texture() {
				glDeleteTextures(1, &id);
			}
			data_texture(const data_texture&) = delete;
			data_texture* operator=(const data_texture&) = delete;
			std::pair<GLenum,GLenum> ft_via_T() {
				GLenum fmt, type;
				if      (std::is_same<T,vec4>::value)  fmt = GL_RGBA, type = GL_FLOAT;
				else if (std::is_same<T,vec3>::value)  fmt = GL_RGB,  type = GL_FLOAT;
				else if (std::is_same<T,float>::value) fmt = GL_RED,  type = GL_FLOAT;
				else throw std::logic_error(std::string("incomplete list of tex formats in ") + __PRETTY_FUNCTION__ + "@" + __FILE__);
				return {fmt, type};
			}
			void resize(int new_w, int new_h) {
				auto [fmt,type] = ft_via_T();
				glBindTexture(GL_TEXTURE_2D, id);
				glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w=new_w, h=new_h, 0, fmt, type, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glBindTexture(GL_TEXTURE_2D, 0);
				org_data.clear();
			}
			void bind(int unit) {
				glActiveTexture(GL_TEXTURE0+unit);
				glBindTexture(GL_TEXTURE_2D, id);
			}
			void unbind(int unit) {
				glActiveTexture(GL_TEXTURE0+unit);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
			void bind_as_image(int unit, bool read, bool write) {
				GLenum access;
				if (read && write) access = GL_READ_WRITE;
				else if (read)     access = GL_READ_ONLY;
				else if (write)    access = GL_WRITE_ONLY;
				assert(read || write);
				glBindImageTexture(unit, id, 0, GL_FALSE, 0, access, internal_format);
			}
			void unbind_as_image(int unit) {
				glBindImageTexture(unit, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
			}
			void download() {
				if (org_data.size() != w*h)
					org_data.resize(w*h);
				auto [fmt,type] = ft_via_T();
				glBindTexture(GL_TEXTURE_2D, id);
				glGetTexImage(GL_TEXTURE_2D, 0, fmt, type, org_data.data());
				glBindTexture(GL_TEXTURE_2D, 0);
			}
		};
		// RAII wrapper
		template<typename T> struct bind_texture_as_image {
			T &tex;
			int unit;
			bind_texture_as_image(T &tex, int unit, bool read, bool write) : tex(tex), unit(unit) {
				tex.bind_as_image(unit, read, write);
			}
			~bind_texture_as_image() {
				tex.unbind_as_image(unit);
			}
		};

		struct rng {
			ssbo<uint64_t> pcg_data;
			rng(const std::string &name);
			void init_pcg_data_host(int w, int h);
		};

		
		/*! \brief Ray data, including intersection points
		 *
		 *  The indices given for the Shader Storage Buffer Objects (ssbo) correspond to the binding locations in the
		 *  shaders (\see compute_shader).
		 *  To make sure that a given raydata instance is bound to those indices, call `bind'.
		 *  
		 *  Note: If we add different ray layouts this might be called raydata_soa and raydata be the name of the
		 *  platform-specific base class referenced in \ref gl::platform. Invidividual ray tracers would then hold the
		 *  appropriate pointer to their ray data
		 */
		struct raydata : public wf::raydata {
			int w, h;
			data_texture<vec4> rays;
			data_texture<vec4> intersections;
			data_texture<vec4> framebuffer;

			raydata(glm::ivec2 dim) : raydata(dim.x, dim.y) {
			}
			raydata(int w, int h)
			: w(w), h(h),
			  rays("rays", w, h*3, GL_RGBA32F),
			  intersections("intersections", w, h, GL_RGBA32F),
			  framebuffer("framebuffer", w, h, GL_RGBA32F) {
				  rc->call_at_resolution_change[this] = [this](int w, int h) {
					  this->w = w;
					  this->h = h;
					  rays.resize(w,h*3);
					  intersections.resize(w,h);
					  framebuffer.resize(w,h);
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
			ssbo<vec4> texture_data_hacky;
			scenedata()
			: vertices("vertices", BIND_VERT, 0),
			  triangles("triangles", BIND_TRIS, 0),
			  materials("materials", BIND_MTLS, 0),
			  texture_data_hacky("texture_data", BIND_TEXD, 0) {
			}
			~scenedata();
			void upload(scene *scene);
		};

		
		/*! \brief Default OpenGL Ray Tracer.
		 *
		 */
		struct batch_rt : public batch_ray_tracer {
			gl::raydata *rd = nullptr;
			virtual void build(scenedata *scene) {
				rd = new raydata(rc->resolution());
			}
			void use(wf::raydata *rays) override { 
			    rd = dynamic_cast<raydata*>(rays);
			}
		};

	}
}
