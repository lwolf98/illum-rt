#pragma once

#include <GL/glew.h>
#include <string>

extern std::string headless_render_device;

enum gl_mode { gl_truly_headless, gl_glfw_headless };

void initialize_opengl_context(::gl_mode gl_mode, int major, int minor);
bool gl_variant_available(::gl_mode gl_mode);

