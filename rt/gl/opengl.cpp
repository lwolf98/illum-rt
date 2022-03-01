#include "config.h"
#include "opengl.h"

#include <stdexcept>
#include <iostream>

#ifdef HAVE_GL
#include <GL/glew.h>
#endif

#ifdef HAVE_LIBGLFW
#define HAVE_GLFW
#endif

#if defined(HAVE_LIBEGL) && defined(HAVE_LIBGBM)
#define HAVE_HEADLESSGL
#endif

#ifdef HAVE_HEADLESSGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <assert.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef HAVE_GLFW
#include <GLFW/glfw3.h>
		
static GLFWwindow *window = nullptr;
#endif

//std::string headless_render_device = "/dev/dri/renderD128";
std::string headless_render_device = "/dev/dri/card0";

#ifdef HAVE_HEADLESSGL
void init_generic_buffer_managed_headless_gl(int major, int minor) {
	std::cout << "Setting up truly headless OpenGL context" << std::endl;
	int32_t fd = open(headless_render_device.c_str(), O_RDWR);
	assert(fd > 0);

	struct gbm_device *gbm = gbm_create_device(fd);
	assert(gbm != nullptr);

	/* setup EGL from the GBM device */
	EGLDisplay egl_dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm, nullptr);
	assert(egl_dpy != nullptr);

	bool res = eglInitialize(egl_dpy, nullptr, nullptr);
	assert(res);

	const char *egl_extension_st = eglQueryString(egl_dpy, EGL_EXTENSIONS);
	assert(strstr(egl_extension_st, "EGL_KHR_create_context") != nullptr);
	assert(strstr(egl_extension_st, "EGL_KHR_surfaceless_context") != nullptr);

	static const EGLint config_attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, //EGL_OPENGL_ES3_BIT_KHR,
		EGL_NONE
	};
	EGLConfig cfg;
	EGLint count;

	res = eglChooseConfig(egl_dpy, config_attribs, &cfg, 1, &count);
	assert(res);

	res = eglBindAPI(EGL_OPENGL_API);
	assert(res);

	static const EGLint attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_CONTEXT_MAJOR_VERSION, major,
		EGL_CONTEXT_MINOR_VERSION, minor,
		EGL_NONE
	};
	EGLContext core_ctx = eglCreateContext(egl_dpy,
										   cfg,
										   EGL_NO_CONTEXT,
										   attribs);
	assert(core_ctx != EGL_NO_CONTEXT);

	res = eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, core_ctx);
	assert(res);
	glewInit();
}
#endif

#ifdef HAVE_GLFW
void init_glfw_headless_gl(int major, int minor) {
	std::cout << "Setting up GLFW based OpenGL context without a window" << std::endl;
	if (!glfwInit())
		throw std::runtime_error("cannot create glfw window required for fallback opengl context");
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	window = glfwCreateWindow(1, 1, "Rasterizer", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		throw std::runtime_error("cannot create glfw window required for fallback opengl context");
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);

	if (glewInit() != GLEW_OK) {
		glfwTerminate();
		throw std::runtime_error("cannot initialize glew");
	}
}
#endif

void initialize_opengl_context(::gl_mode gl_mode, int major, int minor) {
#ifndef HAVE_GL
	throw std::runtime_error("This version of RTGI was not compiled with OpenGL support");
#endif
	if (gl_mode == gl_truly_headless) {
#ifndef HAVE_HEADLESSGL
		throw std::runtime_error("This version of RTGI was not compiled with truly headless OpenGL support (try gl_glfw_headless)");
#else
		init_generic_buffer_managed_headless_gl(major, minor);
#endif
	}
	else if (gl_mode == gl_glfw_headless) {
#ifndef HAVE_GLFW
		throw std::runtime_error("This version of RTGI was not compiled with GLFW support (try gl_truly_headless)");
#else
		init_glfw_headless_gl(major, minor);
#endif
	}
	else {
		throw std::logic_error("Invalid OpenGL mode selected");
	}
}

bool gl_variant_available(::gl_mode gl_mode) {
#ifndef HAVE_GL
	return false;
#endif
	if (gl_mode == gl_truly_headless) {
#ifndef HAVE_HEADLESSGL
		return false;
#else
		return true;
#endif
	}
	else if (gl_mode == gl_glfw_headless) {
#ifndef HAVE_GLFW
		return false;
#else
		return true;
#endif
	}
	else {
		throw std::logic_error("Invalid OpenGL mode selected");
	}
}
