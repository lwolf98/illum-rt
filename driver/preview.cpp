#include "preview.h"

#include "config.h"

#include "libgi/global-context.h"
#include "libgi/gl/shader.h"

#include "interaction.h"
#include "cmdline.h"

#include <stdexcept>
#include <thread>
#include <iostream>
#include <chrono>

#include <GL/glew.h>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

#define GLM_ENABLE_EXPERIMENTAL

using namespace gl;

GLFWwindow *preview_window = nullptr;
GLFWwindow *render_window = nullptr;
gl::ssbo<glm::vec4> *preview_framebuffer = nullptr;
bool preview_update_in_progress = true, preview_finalized = false;
double delta_time = 0;

static render_shader *preview_shader;
static bool update_res = true, alternative_display_mode = true;
static double old_xpos, old_ypos;
static float speed_factor = 0.02f;

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
	if (window != preview_window) return;

	if (button == GLFW_MOUSE_BUTTON_LEFT)
		if (action == GLFW_PRESS) {
			glfwGetCursorPos(window, &old_xpos, &old_ypos);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	if (window != preview_window) return;

	if (key == GLFW_KEY_C && action == GLFW_PRESS && mods == GLFW_MOD_CONTROL)
		glfwSetWindowShouldClose(preview_window, GLFW_TRUE);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(preview_window, GLFW_TRUE);
	if (key == GLFW_KEY_P && action == GLFW_PRESS)
		if(!preview_finalized)
			std::cerr << "WARNING: Wait for frame to finalize before saving" << std::endl;
		else
			rc->framebuffer.png().write(cmdline.outfile);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	speed_factor = std::max((float) yoffset * 0.005 + speed_factor, 0.001);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

static void move_view(){
	float speed = speed_factor * delta_time;
	glm::vec3 pos = rc->scene.camera.pos, new_pos = rc->scene.camera.pos;
	glm::vec3 dir = rc->scene.camera.dir, new_dir = rc->scene.camera.dir;
	glm::vec3 up = rc->scene.camera.up;
	glm::vec3 pos_diff(0);

	if (glfwGetKey(preview_window, GLFW_KEY_W) == GLFW_PRESS) pos_diff += glm::normalize(dir);
	if (glfwGetKey(preview_window, GLFW_KEY_S) == GLFW_PRESS) pos_diff -= glm::normalize(dir);
	if (glfwGetKey(preview_window, GLFW_KEY_A) == GLFW_PRESS) pos_diff -= glm::normalize(glm::cross(dir, up));
	if (glfwGetKey(preview_window, GLFW_KEY_D) == GLFW_PRESS) pos_diff += glm::normalize(glm::cross(dir, up));
	if (glfwGetKey(preview_window, GLFW_KEY_R) == GLFW_PRESS) pos_diff += glm::normalize(up);
	if (glfwGetKey(preview_window, GLFW_KEY_F) == GLFW_PRESS) pos_diff -= glm::normalize(up);

	// The alternative display stretches the color value of the first pixels sample over all the whole preview offset until a sample was calculated for every pixel.
	if (glfwGetKey(preview_window, GLFW_KEY_1) == GLFW_PRESS) alternative_display_mode = true;
	if (glfwGetKey(preview_window, GLFW_KEY_2) == GLFW_PRESS) alternative_display_mode = false;

	if (glm::length(pos_diff) > 0) new_pos += glm::normalize(pos_diff) * speed;

	double xpos, ypos;
	glfwGetCursorPos(preview_window, &xpos, &ypos);

	if (glfwGetMouseButton(preview_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		double delta_x = old_xpos - xpos;
		double delta_y = old_ypos - ypos;
		float sensitivity = 0.00005f;

		if (delta_x) {
			new_dir = glm::rotate(dir, (float) (delta_x * sensitivity * delta_time), glm::normalize(up));
			old_xpos = xpos;
		}

		if (delta_y) {
			new_dir = glm::rotate(new_dir, (float) (delta_y * sensitivity * delta_time), glm::normalize(glm::cross(new_dir, up)));
			old_ypos = ypos;
		}

		if (delta_x || delta_y) new_dir = glm::normalize(new_dir);
	}

	if (new_pos != pos) queue_command("at "   + std::to_string(new_pos.x) + " " + std::to_string(new_pos.y) + " " + std::to_string(new_pos.z));
	if (new_dir != dir) queue_command("look " + std::to_string(new_dir.x) + " " + std::to_string(new_dir.y) + " " + std::to_string(new_dir.z));
	if (new_pos != pos || new_dir != dir) {
		preview_update_in_progress = true;
		queue_command("run", remove_prev_same_commands);
	}
}

void preview_render_setup() {

	static const float vertex_data[] = {
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,   // lb
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // rb
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,	  // rt
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f }; // lt

	unsigned int indices[] = {
		0, 1, 2,
		2, 3, 0 };

	const std::string vertex_shader_code =
		R"(
		#version 440
		layout(location = 0) in vec3 pos;
		layout(location = 1) in vec2 in_tex_coord;
		out vec2 tex_coord;
		void main()
		{
			tex_coord = in_tex_coord;
			gl_Position.xyz = pos;
			gl_Position.w = 1.0;
		}
		)";

	const std::string fragment_shader_code =
		R"(
		#version 440
		layout (std430, binding = 7) buffer b_frambuffer { vec4 framebuffer []; };
		in vec2 tex_coord;
		out vec4 color;
		uniform int w;
		uniform int h;
		uniform int offset;
		uniform int sample_count;
		uniform int alternative_display_mode;

		uniform ivec2 render_offset;
		void main()
		{
			int x = int(tex_coord.x * w);
			int y = int(tex_coord.y * h);

			if(sample_count == 0 && bool(alternative_display_mode)) {
				x = x / offset * offset;
				y = y / offset * offset;
			}

			int pos = y * w + x;
			color =  framebuffer[pos] / framebuffer[pos].w;
		}
		)";

	// gamma correction
	glEnable(GL_FRAMEBUFFER_SRGB); 

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(float), vertex_data, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (const void *)(3 * sizeof(float)));

	GLuint ibo;
	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned int), indices, GL_STATIC_DRAW);

	preview_shader = new render_shader("preview", vertex_shader_code, fragment_shader_code);
	preview_shader->bind();

	preview_framebuffer = new gl::ssbo<glm::vec4>("framebuffer", 7, 1);
}

void update_resolution(glm::ivec2 res) {
	glfwSetWindowSize(preview_window, res.x, res.y);
	preview_framebuffer->resize(res.x * res.y);
	preview_shader->uniform("w", res.x);
	preview_shader->uniform("h", res.y);
	update_res = false;
}

void render_preview() {

	glfwMakeContextCurrent(preview_window);
	glfwSwapInterval(1);

	if (cmdline.verbose) {
		std::cout << "OpenGL context acquired:" << std::endl;
		std::cout << "- OpenGL version: " << glGetString(GL_VERSION) << ", " << glGetString(GL_RENDERER) << std::endl;
		std::cout << "- GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
		std::cout << "- Vendor: " << glGetString(GL_VENDOR) << std::endl;
		std::cout << "- Renderer: " << glGetString(GL_RENDERER) << std::endl;
	}

	if (glewInit() != GLEW_OK) {
		std::cerr << "glew init failed for preview window" << std::endl;
		return;
	}

	preview_render_setup();

	rc->call_at_resolution_change[&preview_window] = [&](int w, int h) {
		update_res = true;
	};
	
	{
		glm::ivec2 res = rc->resolution();
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode *mode = glfwGetVideoMode(monitor);
		glfwSetWindowPos(preview_window, (mode->width-res.x)/2, (mode->height-res.y)/2);
		update_resolution(res);
	}

	while (!glfwWindowShouldClose(preview_window)) {
		if (update_res)
			update_resolution(rc->resolution());

		if(rc->algo) {
			preview_shader->uniform("sample_count", (int)rc->algo->current_sample_index);
			preview_shader->uniform("alternative_display_mode", alternative_display_mode);
			preview_shader->uniform("offset", (int)rc->preview_offset);
		}

		move_view();

		glClear(GL_COLOR_BUFFER_BIT);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
		glfwSwapBuffers(preview_window);

		glfwPollEvents();
	}

	queue_command("exit");
	glfwDestroyWindow(preview_window);
	preview_window = nullptr; // glfwDestroyWindow does not set the window pointer to nullptr
}

/*! \brief We call this on the main thread after we closed the preview and all commands in the command queue have been processed
 *  so we don't access the preview framebuffer after it has been deleted.
*/
void terminate_gl() {
	if (preview_window) throw std::runtime_error("can not terminate gl as the preview window is still active");

	delete preview_shader;
	delete preview_framebuffer;
	glfwTerminate();
}

/*! \brief We always call this when we run with the preview window.
 *  If a tracer implementation depends on GLFW it has to check if it is already active
 *  If not it needs to call this function itself. (see \ref init_glfw_headless_gl)
*/
void init_glfw() {
	if (!glfwInit()) throw std::runtime_error("can not initialize glfw");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_VERSION_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_VERSION_MINOR);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // do not put in tile (in a tiling WM)
}

/*! \brief Here we create two GL contexts on the main thread which share resources.
 *  One is used in rendering the preview, the other is used in the thread which traces the images.
 *  We need a seperate context to access buffers, textures etc. on a different thread
 *  as a GL context may only be active on one thread at a time.
 */
void init_preview() {
	init_glfw();

	preview_window = glfwCreateWindow(1, 1, "RTGI live", nullptr, nullptr);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	render_window = glfwCreateWindow(1, 1, "render", nullptr, preview_window);

	if (!preview_window || !render_window) throw std::runtime_error("Can not create preview or render window");

	glfwSetMouseButtonCallback(preview_window, mouse_button_callback);
	glfwSetKeyCallback(preview_window, key_callback);
	glfwSetFramebufferSizeCallback(preview_window, framebuffer_size_callback);
	glfwSetScrollCallback(preview_window, scroll_callback);
}
