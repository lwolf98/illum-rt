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
	
	struct ray_and_intersection_processing : public step {
		virtual void use(batch_ray_tracer *rt) = 0;
		virtual void run() = 0;
	};

	struct raydata {
		virtual ~raydata() {}
	};
	
	#define register_batch_rt(N,C,X) tracers[N] = [C]() -> wf::batch_ray_tracer* { return new X; }
	#define register_rni_step(N,C,X) rnis[N] = [C]() -> wf::ray_and_intersection_processing* { return new X; }
	#define register_rni_step_by_id(C,X) rnis[X::id] = [C]() -> wf::ray_and_intersection_processing* { return new X; }
	class platform {
	protected:
		std::string name;
		std::map<std::string, std::function<batch_ray_tracer*()>> tracers;
		std::map<std::string, std::function<ray_and_intersection_processing*()>> rnis;

		std::set<std::string> tracer_links;
		std::set<std::string> rni_links;

		std::map<std::string, batch_ray_tracer*> generated_tracers;
		std::map<std::string, ray_and_intersection_processing*> generated_rnis;

		void link_tracer(const std::string &existing, const std::string &linkname);

	public:
		platform(const std::string &name) : name(name) {}
		virtual ~platform();

// 		wf::raydata *raydata = nullptr;
		batch_ray_tracer *selected_tracer = nullptr;
		wf::timer *timer = nullptr;

		batch_ray_tracer* select(const std::string &name);
		ray_and_intersection_processing* rni(const std::string &name);
		virtual bool interprete(const std::string &command, std::istringstream &in) { return false; }
	};

	extern std::vector<platform*> platforms;

	class simple_algorithm : public wavefront_algorithm {
	protected:
		std::vector<step*> steps;
	};

	// partition in pure interface and templated version holding a reference to the data
	struct batch_ray_tracer : public ray_tracer {
		ray *rays = nullptr;
		triangle_intersection *intersections = nullptr;

		virtual void compute_closest_hit() = 0;
		virtual void compute_any_hit() = 0;
	};

	class find_closest_hits : public step {
		batch_ray_tracer *rt;
		static constexpr char id[] = "find closest hit";
	public:
		find_closest_hits(batch_ray_tracer *rt) : rt(rt) {}
		void run() override {
			time_this_wf_step;
			rt->compute_closest_hit();
		}
	};
	class find_any_hits : public step {
		batch_ray_tracer *rt;
		static constexpr char id[] = "find any hit";
	public:
		find_any_hits(batch_ray_tracer *rt) : rt(rt) {}
		void run() override {
			time_this_wf_step;
			rt->compute_any_hit();
		}
	};
	
}
