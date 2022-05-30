#pragma once

#include "rt.h"

#include "global-context.h"
#include "context.h"
#include "camera.h"
#include "timer.h"

#include <map>
#include <set>

namespace wf {

	/*! Wavefront tracers will be implemented in different, asynchroneous systems and some of those
	 *  will perform better if timer "events" are inserted where appropriate and synchronized after
	 *  the algorithm has executed.
	 *  To ensure this fact is taken note of we require that /every/ wf::timer override the 
	 *  synchronize function
	 *
	 */
	struct timer : public ::timer {
		virtual void synchronize() = 0;
	};

	/* This define assumes that /each/ wf step that is executed has a member called 'id'.
	 *
	 */
	#define time_this_wf_step raii_timer local_wf_raii_timer(id, *rc->platform->timer)

	struct step {
		virtual void run() = 0;
	};
	
	struct raydata {
		virtual ~raydata() {}
	};
	
	#define register_batch_rt(N,C,X) tracers[N] = [C]() -> wf::batch_ray_tracer* { return new X; }
	#define register_wf_step(N,C,X) steps[N] = [C]() -> wf::step* { return new X; }
	#define register_wf_step_by_id(C,X) steps[X::id] = [C]() -> wf::step* { return new X; }
	class platform {
	protected:
		std::map<std::string, std::function<batch_ray_tracer*()>> tracers;
		std::map<std::string, std::function<wf::step*()>> steps;

		std::set<std::string> tracer_links;
		std::set<std::string> step_links;

		std::map<std::string, batch_ray_tracer*> generated_tracers;
		std::map<std::string, wf::step*> generated_steps;

		void link_tracer(const std::string &existing, const std::string &linkname);

		std::vector<wf::step*> scene_steps;

	public:
		std::string name;
		platform(const std::string &name) : name(name) {}
		virtual ~platform();

// 		wf::raydata *raydata = nullptr;
		batch_ray_tracer *selected_tracer = nullptr;
		wf::timer *timer = nullptr;

		batch_ray_tracer* select(const std::string &name);
		wf::step* step(const std::string &name);
		virtual void commit_scene(scene *scene) = 0;
		virtual bool interprete(const std::string &command, std::istringstream &in) { return false; }
		
		void append_setup_step(wf::step *s) {
			scene_steps.push_back(s);
		}
	};

	extern std::vector<platform*> platforms;

	class simple_algorithm : public wavefront_algorithm {
	protected:
		std::vector<step*> data_reset_steps;
		std::vector<step*> frame_preparation_steps;
		std::vector<step*> sampling_steps;
		std::vector<step*> frame_finalization_steps;
	public:
		void prepare_data() override {
			for (auto *s : data_reset_steps) s->run();
		}
		void prepare_frame() override {
			wavefront_algorithm::prepare_frame();
			for (auto *s : frame_preparation_steps) s->run();
		}
		void compute_samples() override {
			for (int i = 0; i < rc->sppx; ++i) {
				for (auto *step : sampling_steps)
					step->run();
				rc->platform->timer->synchronize();
			}
		}
		void finalize_frame() override {
			for (auto *s : frame_finalization_steps) s->run();
			rc->platform->timer->synchronize();
		}
	};

	// partition in pure interface and templated version holding a reference to the data
	struct batch_ray_tracer : public ray_tracer {
		ray *rays = nullptr;
		triangle_intersection *intersections = nullptr;

		virtual void compute_closest_hit() = 0;
		virtual void compute_any_hit() = 0;
	};

	class find_closest_hits : public step {
	protected:
		batch_ray_tracer *rt;
	public:
		static constexpr char id[] = "find closest hit";
		find_closest_hits(batch_ray_tracer *rt) : rt(rt) {}
		void run() override {
			time_this_wf_step;
			rt->compute_closest_hit();
		}
	};
	class find_any_hits : public step {
	protected:
		batch_ray_tracer *rt;
	public:
		static constexpr char id[] = "find any hit";
		find_any_hits(batch_ray_tracer *rt) : rt(rt) {}
		void run() override {
			time_this_wf_step;
			rt->compute_any_hit();
		}
	};
	struct build_accel_struct : public step {
		static constexpr char id[] = "build accel struct";
	};
	struct compute_light_distribution : public step {
		static constexpr char id[] = "compute light distribution";
	};
	struct sample_uniform_light_directions : public step {
		static constexpr char id[] = "sample uniform light directions";
	};
}
