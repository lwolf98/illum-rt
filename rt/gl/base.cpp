#include "base.h"
#include "rni.h"
#include "seq.h"

#include <stdexcept>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

namespace wf {
	namespace gl {

		static GLFWwindow *window;

		platform::platform() : wf::platform("opengl") {
			if (!glfwInit())
				throw std::runtime_error("cannot create glfw window required for fallback opengl context");
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
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
			
			
			tracers["default"] = new seq_tri_is;
			// bvh mode?
			rnis["setup camrays"] = new batch_cam_ray_setup;
			rnis["store hitpoint albedo"] = new store_hitpoint_albedo;
		}
		
	}
}
