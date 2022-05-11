#include "platform.h"

#include "base.h"
#include "find-hit.h"
#include "opengl.h"
#include "rni.h"

#include <iostream>

using namespace std;

#define check_in(x) { if (in.bad() || in.fail()) cerr << "error in command: " << (x) << endl; }

namespace wf::gl {
	
	platform::platform(const std::vector<std::string> &args) : wf::platform("opengl") {
		if (pf) std::logic_error("The " + name + " platform is already set up");
		pf = this;

		gl_mode requested_mode = gl_truly_headless;
		for (auto arg : args)
			if (arg == "gbm" || arg == "truly-headless") requested_mode = gl_truly_headless;
			else if (arg == "glfw" || arg == "with-X") requested_mode = gl_glfw_headless;
			else if (arg == "notex") texture_support_mode = NO_TEX;
			else
				std::cerr << "Platform opengl does not support the argument " << arg << std::endl;
		if (gl_variant_available(requested_mode))
			initialize_opengl_context(requested_mode, 4, 4);
		else if (requested_mode != gl_glfw_headless)
			initialize_opengl_context(gl_glfw_headless, 4, 4);

		enable_gl_debug_output();

		if (texture_support_mode == PROPER_BINDLESS)
			if (!GLEW_ARB_bindless_texture) {
				cerr << "You GPU or GL-version does not support ARB_bindless_texture, so textured materials will not work as expected" << endl;
				texture_support_mode = NO_TEX;
			}
			else if (!GLEW_NV_gpu_shader5) {
				cerr << "Your GPU only supports bindless textures with dynamically uniform access pattern, which our shaders do not produce, so textured materials will rely on a hacky implementation with probably lower performance" << endl;
				texture_support_mode = HACKY;
			}

		register_batch_rt("seq",, seq_tri_is);
		register_batch_rt("bbvh-1",, bvh);
		link_tracer("bbvh-1", "default");
		// 			link_tracer("seq", "default");
		// bvh mode?
		register_wf_step_by_id(, initialize_framebuffer);
		register_wf_step_by_id(, batch_cam_ray_setup);
		register_wf_step_by_id(, add_hitpoint_albedo);
		register_wf_step_by_id(, download_framebuffer);
		register_wf_step_by_id(, find_closest_hits);
		register_wf_step_by_id(, find_any_hits);

		timer = new wf::gl::timer;
	}

	platform::~platform() {
		pf = nullptr;
	}
		
	void platform::commit_scene(::scene *scene) {
		delete pf->sd;
		pf->sd = new scenedata;
		
		pf->sd->upload(scene);
		if (!rt)
			rt = dynamic_cast<batch_rt*>(select("default"));
		scene->compute_light_distribution(); // TODO extract as step
		rt->build(pf->sd);
	}

	bool platform::interprete(const std::string &command, std::istringstream &in) { 
		if (command == "raytracer") {
			string variant;
			in >> variant;
			check_in("Syntax error, requires opengl ray tracer variant name");
			//TODO rc->scene.use(select(variant));
			throw "fixme";
			return true;
		}
		return false;
	}

	platform *pf = nullptr;
}
