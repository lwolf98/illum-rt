#include "platform.h"
#include "base.h"

#include "rni.h"
#include "tracers.h"
#include "preprocessing.h"

#include <iostream>
using namespace std;

#define error(x) { cerr << "command (" << command << "): " << x << endl;  return true; }
#define check_in_complete(x) { if (in.bad() || in.fail() || !in.eof()) error(x); }

namespace wf {
	namespace cuda {
		
		platform::platform(const std::vector<std::string> &args) : wf::platform("cuda") {
			if (pf) std::logic_error("The " + name + " platform is already set up");
			pf = this;

			for (auto arg : args)
				cerr << "Platform cuda does not support the argument " << arg << endl;
			register_batch_rt("simple",, simple_rt);
			register_batch_rt("if-if",, ifif);
			register_batch_rt("while-while",, whilewhile);
			register_batch_rt("persistent-if-if",, persistentifif);
			register_batch_rt("persistent-while-while",, persistentwhilewhile);
			register_batch_rt("speculative-while-while",, speculativewhilewhile);
			register_batch_rt("persistent-speculative-while-while",, persistentspeculativewhilewhile);
			register_batch_rt("dynamic-while-while",, dynamicwhilewhile);

			link_tracer("while-while", "default");
			link_tracer("while-while", "find closest hits");
			// bvh mode?
			register_wf_step_by_id(, initialize_framebuffer);
			register_wf_step_by_id(, batch_cam_ray_setup);
			//register_wf_step("store hitpoint albedo",, store_hitpoint_albedo_cpu);
			register_wf_step_by_id(, add_hitpoint_albedo_to_fb);
			register_wf_step_by_id(, download_framebuffer);
			register_wf_step_by_id(, find_closest_hits);
			register_wf_step_by_id(, find_any_hits);
			register_wf_step_by_id(, rotate_scene);
			register_wf_step_by_id(, rotate_scene_keep_org);
			register_wf_step_by_id(, build_accel_struct);
			register_wf_step_by_id(, drop_scene_view);

			timer = new wf::cuda::timer;
		}

		platform::~platform() {
			cudaDeviceReset();
			pf = nullptr;
		}
	
		void platform::commit_scene(::scene *scene) {
			while (pf->sd) {
				auto *scene_or_view = pf->sd;
				pf->sd = pf->sd->org;
				delete scene_or_view;
			}
			pf->sd = new scenedata;
			pf->sd->upload(scene);

			if (!rt)
				rt = dynamic_cast<batch_rt*>(select("default"));
			scene->compute_light_distribution(); // TODO extract as step

			for (auto step : scene_steps)
				step->run();
		}

		bool platform::interprete(const std::string &command, std::istringstream &in) { 
			if (command == "raytracer") {
				string variant;
				in >> variant;
				check_in_complete("Syntax error, requires (for now, only) cuda ray tracer variant name");
				if      (variant == "simple")                             rt = dynamic_cast<batch_rt*>(select("simple"));
				else if (variant == "if-if")                              rt = dynamic_cast<batch_rt*>(select("if-if"));
				else if (variant == "while-while")                        rt = dynamic_cast<batch_rt*>(select("while-while"));
				else if (variant == "persistent-if-if")                   rt = dynamic_cast<batch_rt*>(select("persistent-if-if"));
				else if (variant == "persistent-while-while")             rt = dynamic_cast<batch_rt*>(select("persistent-while-while"));
				else if (variant == "speculative-while-while")            rt = dynamic_cast<batch_rt*>(select("speculative-while-while"));
				else if (variant == "persistent-speculative-while-while") rt = dynamic_cast<batch_rt*>(select("persistent-speculative-while-while"));
				else if (variant == "dynamic-while-while")                rt = dynamic_cast<batch_rt*>(select("dynamic-while-while"));
				else error("There is no such cuda ray tracer variant");
				return true;
			}
			return false;
		}
		
		platform *pf = nullptr;

	}
}

