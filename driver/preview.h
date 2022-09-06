#pragma once

#include "rt/gl/base.h"

#include <GLFW/glfw3.h>

#define OPENGL_VERSION_MAJOR 4
#define OPENGL_VERSION_MINOR 4

void init_glfw();
void init_preview();
void render_preview();
void terminate_gl();

extern GLFWwindow *preview_window;
extern GLFWwindow *render_window;
extern wf::gl::ssbo<glm::vec4> *preview_framebuffer;