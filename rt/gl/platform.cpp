#include "platform.h"

#include "base.h"
#include "find-hit.h"
#include "opengl.h"
#include "rni.h"
#include "preprocessing.h"
#include "bounce.h"

#include "config.h"

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
				cerr << "Your GPU or GL-version does not support ARB_bindless_texture, but your what we found during configure time suggested otherwise. " << endl
				     << "Texturing will be disabled." << endl;
				texture_support_mode = NO_TEX;
			}
			else if (!GLEW_NV_gpu_shader5) {
				cerr << "Your GPU only supports bindless textures with dynamically uniform access pattern, which our shaders do not produce" << endl
				 	 << "However, what we found during configure time suggested otherwise. " << endl
				     << "Texturing will be disabled." << endl;
				texture_support_mode = NO_TEX;
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
		register_wf_step_by_id(, build_accel_struct);
		register_wf_step_by_id(, sample_uniform_dir);
		register_wf_step_by_id(, sample_cos_weighted_dir);
		register_wf_step_by_id(, compute_light_distribution);
		register_wf_step_by_id(, sample_light_dir);
		register_wf_step_by_id(, integrate_dir_sample);
		register_wf_step_by_id(, integrate_light_sample);
		register_wf_step_by_id(, copy_to_preview);

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

		for (auto step : scene_steps)
			step->run();
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
		
	raydata* platform::allocate_raydata() {
	    return new raydata(rc->resolution());
	}

	per_sample_data<float>* platform::allocate_float_per_sample() {
		return new per_sample_data<float>(rc->resolution());
	}

	per_sample_data<vec3>* platform::allocate_vec3_per_sample() {
		return new per_sample_data<vec3>(rc->resolution());
	}

	platform *pf = nullptr;
}
