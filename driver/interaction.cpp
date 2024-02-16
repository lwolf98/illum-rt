/*
 * 	Defindes the user-interaction via shell prompt and script file.
 * 	
 * 	Would be nice to extend with readline capabilities.
 *
 */
#include "interaction.h"
#include "config.h"

#include "cmdline.h"

#include "libgi/timer.h"
#include "libgi/scene.h"
#include "libgi/algorithm.h"
#include "libgi/framebuffer.h"
#include "libgi/context.h"

#ifndef RTGI_SKIP_WF
#include "libgi/wavefront-rt.h"

#include "rt/cpu/platform.h"
#endif

#include "rt/cpu/seq.h"
#ifndef RTGI_SKIP_BVH
#include "rt/cpu/bvh.h"
#endif
#include "gi/primary-hit.h"
#ifndef RTGI_SKIP_DIRECT_ILLUM
#include "gi/direct.h"
#endif
#ifndef RTGI_SKIP_SIMPLE_PT
#include "gi/pt.h"
#endif

#ifdef HAVE_GL
#ifndef RTGI_SKIP_WF
#include "rt/gl/platform.h"
#endif
#include "preview.h"
#endif

#ifndef RTGI_SKIP_WF
#ifdef HAVE_CUDA
#include "rt/cuda/platform.h"
#endif
#endif

#ifndef RTGI_SKIP_BVH
#ifdef HAVE_LIBEMBREE3
#include "rt/cpu/embree.h"
#endif
#endif

#include "libgi/timer.h"

#include "libgi/global-context.h"

#include <iostream>
#include <sstream>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <glm/glm.hpp>
#if GLM_VERSION < 997
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/string_cast.hpp>

using namespace glm;
using namespace std;

inline istream& operator>>(istream &in, vec3 &x) {
	in >> x.x >> x.y >> x.z;
	return in;
}

static string remove_surrounding_space(const std::string &str) {
	string s = str;
    s.erase(s.find_last_not_of("\t ") + 1, string::npos);
    s.erase(0, std::min(s.find_first_not_of("\t "), str.size() - 1));
	return s;
}

static string read_rest_of_line(std::istream &in) {
	string name;
	getline(in, name);
	return remove_surrounding_space(name);
}

const char *prompt = "rtgi > ";

#define ifcmd(c) if (command==c)
#define error_no_return(x) { cerr << "command " << uc.cmdid << " (" << command << "): " << x << endl; }
#define error(x) { error_no_return(x);  return; }
#define check_in(x) { if (in.bad() || in.fail()) error(x); }
#define check_in_complete(x) { if (in.bad() || in.fail() || !in.eof()) error(x); }

void run(gi_algorithm *algo);
#ifdef HAVE_GL
extern float speed_factor;
void run_sample(gi_algorithm *algo);
#endif

/* 
 *  The Read Eval Print Loop is run in a thread and queues all commands in order
 *  (potentially blocking on cin) in the command_queue for evaluation in the main thread.
 *
 *  Reading from cin would be much improved by readline(3) support.
 *
 */

void repl(istream &infile) {
	while (!infile.eof()) {
		if (&infile == &cin)
			cout << prompt << flush;
		string line, command;
		getline(infile, line);
		queue_command(line);
		if (line == "exit" || line == "quit") // this is a hack
			return;
	}
	cout << endl;
}

void run_repls() {
	if (cmdline.script != "") {
		ifstream script(cmdline.script);
		rc->scene.add_modelpath(std::filesystem::path(cmdline.script).parent_path());
		if (script.fail()){
			cerr << "Script file not found" << endl;
			return;
		}
		repl(script);
		rc->scene.remove_modelpath(std::filesystem::path(cmdline.script).parent_path());
	}
	if (cmdline.interact)
		repl(cin);

#ifdef HAVE_GL
	if (!preview_window) queue_command("exit");
#else 
	queue_command("exit");
#endif
}

/* 
 *  Commands can be queued in from anywhere via queue_command().
 *  The commands are taken out of the queue by RTGI's main thread (it is woken up
 *  when new commands are entered into the queue) and then evaluated via eval().
 *
 */

static mutex command_queue_mutex;
static condition_variable queue_ready;
static list<string> command_queue;
static bool expecting_commands = true; // only set via eval, called only via process_command_queue

void eval(const std::string &command);

void queue_command(const std::string &command, enqueue_mode mode) {
	unique_lock lock(command_queue_mutex);
	if (mode == remove_prev_same_commands)
		command_queue.remove(command);
	command_queue.push_back(command);
	lock.unlock();
	queue_ready.notify_one();
}

void process_command_queue() {
	while (expecting_commands) {
		string command = "";
		unique_lock lock(command_queue_mutex);
		if (command_queue.empty())
			queue_ready.wait(lock); // may wake up spuriously
		if (!command_queue.empty()) {
			command = command_queue.front();
			command_queue.pop_front();
		}
		lock.unlock();
		eval(command);
	}
}

/*  
 *  Command evaluation is just a big switch/case
 *
 */

//! Keep track of when the user changed important values we have to know about in other places.
struct repl_update_checks {
	unsigned cmdid = 0,
			 scene_touched_at = 0,
			 tracer_touched_at = 0,
			 accel_touched_at = 0;
	bool valid_platform = true; // even true for "no platform", but false when a platform was requested but is not available.
};
static repl_update_checks uc;
static list<string> command_history;

static mat4 modelmatrix(1);

bool platform_and_algo_aligned() {
#ifndef RTGI_SKIP_WF
	bool wf_algo = dynamic_cast<wavefront_algorithm*>(rc->algo);
	if (wf_algo && rc->platform)
		return true;
	if (!wf_algo && !rc->platform)
		return true;
	return false;
#else
	// This is used in a version of this code that supports different computing platfomrs
	return true;
#endif
}

static map<string,bool> disabled_features;

void eval(const std::string &line) {
	if (command_history.back() != line)
		command_history.push_back(line);

	istringstream in(line);
	uc.cmdid++;
	
	scene &scene = rc->scene;
	static material *selected_mat = nullptr;
	static bool cam_has_pos = false,
				cam_has_dir = false,
				cam_has_up = false,
				scene_up_set = false;

	string command;
	in >> command;
	vec3 tmp;
	static bool skip = false;
	if (skip) {
		if (line == "}")
			skip = false;
		else if (cmdline.verbose)
			cerr << "Warning: Skipping command '" << line << "'" << endl;
		return;
	}
	
	ifcmd("history") {
		command_history.pop_back();
		for (auto &x : command_history)
			cout << x << endl;
	}
	else ifcmd("quit")
		expecting_commands = false;
	else ifcmd("exit")
		expecting_commands = false;
	else ifcmd("disable") {
		string what;
		in >> what;
		check_in_complete("Syntax error, expecting only one feature to disable");
		cout << "DISABLING " << what << endl;
		disabled_features[what] = true;
	}
	else ifcmd("with") {
		bool newval = false;
		string what, rest;
		in >> what;
		if (what == "gl") {
#ifndef HAVE_GL
			newval = true;
#else
			newval = disabled_features[what];
#endif
		}
		else if (what == "cuda") {
#ifndef HAVE_CUDA
			newval = true;
#else
			newval = disabled_features[what];
#endif
		}
		else if (what == "optix") {
#ifndef HAVE_OPTIX
			newval = true;
#else
			newval = disabled_features[what];
#endif
		}
		else
			error("Syntax error: expected one of gl, cuda, optix");
		in >> rest;
		check_in_complete("Syntax error, expecting only {");
		if (rest != "{")
			error("Syntax error, expecting {");
		skip = newval;
		if (skip)
			cout << "Skipping a block due to missing " << what << endl;
	}
	else ifcmd("}") {
		check_in_complete("Syntax error: } must be on an otherwise empty line");
	}
	else ifcmd("at") {
		in.get();
		if (in.eof()) {
			cout << scene.camera.pos << endl;
			return;
		}
		in >> tmp;
		check_in_complete("Syntax error, requires 3 numerical components");
		scene.camera.pos = tmp;
		cam_has_pos = true;
	}
	else ifcmd("look") {
		in.get();
		if (in.eof()) {
			cout << scene.camera.dir << endl;
			return;
		}
		in >> tmp;
		check_in_complete("Syntax error, requires 3 numerical components");
		scene.camera.dir = tmp;
		cam_has_dir = true;
	}
	else ifcmd("up") {
		in.get();
		if (in.eof() && scene_up_set) {
			cout << scene.up << endl;
			return;
		}
		if (scene_up_set)
			error("Cannot set scene up vector twice, did you mean camup?");
		in >> tmp;
		check_in_complete("Syntax error, requires 3 numerical components");
		scene_up_set = true;
		scene.up = tmp;
		if (!cam_has_up) {
			scene.camera.up = scene.up;
			cam_has_up = true;
		}
	}
	else ifcmd("camup") {
		in.get();
		if (in.eof()) {
			cout << scene.camera.up << endl;
			return;
		}
		in >> tmp;
		check_in_complete("Syntax error, requires 3 numerical components");
		scene.camera.up = tmp;
		cam_has_up = true;
	}
	else ifcmd("print-cam") {
		cout << "at "   << scene.camera.pos << endl;
		cout << "look " << scene.camera.dir << endl;
		cout << "up "   << scene.up << endl;
		cout << "up "   << scene.camera.up << endl;
	}
	else ifcmd("modelmatrix") {
		in >> modelmatrix[0][0] >> modelmatrix[0][1] >> modelmatrix[0][2] >> modelmatrix[0][3];
		in >> modelmatrix[1][0] >> modelmatrix[1][1] >> modelmatrix[1][2] >> modelmatrix[1][3];
		in >> modelmatrix[2][0] >> modelmatrix[2][1] >> modelmatrix[2][2] >> modelmatrix[2][3];
		in >> modelmatrix[3][0] >> modelmatrix[3][1] >> modelmatrix[3][2] >> modelmatrix[3][3];
		check_in_complete("Syntax error in model matrix");
	}
	else ifcmd("modeltrafo") {
		string trafo;
		in >> trafo;
		if (trafo == "shift" || trafo == "translate") {
			vec3 s;
			in >> s.x >> s.y >> s.z;
			check_in_complete("Too many components in shift transformation");
			modelmatrix = translate(modelmatrix, s);
		}
		else if (trafo == "rotate") {
			vec3 axis[3] { vec3(1,0,0), vec3(0,1,0), vec3(0,0,1) };
			int aid = 0;
			string a;
			in >> a;
			if (a == "x" || a == "X") aid = 0;
			else if (a == "y" || a == "Y") aid = 1;
			else if (a == "z" || a == "Z") aid = 2;
			else error("Invalid axis of rotation; " << a);
			float deg;
			in >> deg;
			check_in_complete("Rotation requires one angle (in degrees)");
			modelmatrix = rotate(modelmatrix, deg, axis[aid]);
		}
		else if (trafo == "scale") {
			vec3 s;
			in >> s.x >> s.y >> s.z;
			check_in_complete("Too many components in scale transformation");
			modelmatrix = scale(modelmatrix, s);
		}
		else error("unsupported trafo: " << trafo);
	}
	else ifcmd("load") {
		string file, name;
		in >> file;
		if (!in.eof())
			in >> name;
		check_in_complete("Syntax error, requires a file name (no spaces, sorry) and (optionally) a name");
		scene.add(file, name, modelmatrix);
		uc.scene_touched_at = uc.cmdid;
	}
	else ifcmd("modelpath") {
		string name;
		in >> name;
		check_in_complete("Syntax error, requires a path name (no spaces, sorry)");
		scene.add_modelpath(name);
	}
	else ifcmd("resolution") {
		int w, h;
		in >> w >> h;
		check_in_complete("Syntax error, requires 2 integral values");
		rc->change_resolution(w, h);
	}
	else ifcmd("algo") {
		string name;
		in >> name;
		gi_algorithm *a = nullptr;
		if (name == "primary")      a = new primary_hit_display;
#ifndef RTGI_SKIP_DEBUGALGO
		else if (name == "info")    a = new info_display;
#endif
#ifndef RTGI_SKIP_WF
#define select_wf(X) if (!rc->platform) error("Cannot select wf algorithm without an active platform") else a = new X
		else if (name == "primary-wf") {
			select_wf(wf::primary_hit_display);
		}
		else if (name == "direct-wf") {
			select_wf(wf::direct_light);
		}
#undef select_wf
#endif
#ifndef RTGI_SKIP_LOCAL_ILLUM
		else if (name == "local")  a = new local_illumination;
#endif
#ifndef RTGI_SKIP_DIRECT_ILLUM
		else if (name == "direct")  a = new direct_light;
#ifndef RTGI_SKIP_DIRECT_MIS
		else if (name == "direct/mis")  a = new direct_light_mis;
#else
		// todo: set up mis algorithm here
#endif
#endif
#ifndef RTGI_SKIP_SIMPLE_PT
		else if (name == "simple-pt")  a = new simple_pt;
#ifndef RTGI_SKIP_PT
		else if (name == "pt")  a = new pt_nee;
#endif
#endif
		else error("There is no gi algorithm called '" << name << "'");
		if (a) {
			delete rc->algo;
			rc->algo = a;
		}
	}
	else ifcmd("outfile") {
		string name;
		in >> name;
		check_in_complete("Syntax error, only accepts a single file name (no spaces, sorry)");
		cmdline.outfile = name;
	}
// 	else ifcmd("bookmark") {
// 		
// 	}
	else ifcmd("raytracer") {
#ifndef RTGI_SKIP_WF
		if (rc->platform) {
			rc->platform->interprete(command, in);
			return;
		}
#endif
		string name;
		in >> name;
		if (name == "seq") scene.use(new seq_tri_is);
#ifndef RTGI_SKIP_BVH
		else if (name == "naive-bvh") scene.use(new naive_bvh);
		else if (name == "bbvh") {
			#ifndef RTGI_SIMPLER_BBVH
			string tag1, tag2;
			in >> tag1 >> tag2;
			bool flat = true;
			bool esc = false;
			if (tag1 == "indexed" || tag2 == "indexed") flat = false;
			if (tag1 == "esc" || tag2 == "esc") esc = true;
			if (flat && !esc)
				scene.use(new binary_bvh_tracer<bbvh_triangle_layout::flat, bbvh_esc_mode::off>);
			else if (!flat && !esc)
				scene.use(new binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::off>);
			else if (!flat && esc)
				scene.use(new binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>);
			else if (flat && esc)
				error("This combination is technically problematic")
			else
				error("There is no such bbvh variant");
			#else
			string tag;
			in >> tag;
			if (tag == "om")      scene.use(new binary_bvh_tracer(binary_bvh_tracer::om));
			else if (tag == "sm") scene.use(new binary_bvh_tracer(binary_bvh_tracer::sm));
			else error("There is no such bbvh variant");
			#endif
		}
#ifdef HAVE_LIBEMBREE3
#ifndef RTGI_SIMPLER_BBVH
		else if (name == "embree") scene.use(new embree_tracer);
		else if (name == "embree-alpha") scene.use(new embree_tracer<true>);
#endif
#endif
#endif
		else error("There is no ray tracer called '" << name << "'");
		uc.tracer_touched_at = uc.cmdid;
	}
#ifndef RTGI_SKIP_WF
	else ifcmd("platform") {
		string name;
		in >> name;
		if (name == "") {
			if (rc->platform)
				cout << "platform: " << rc->platform->name << endl;
			else
				cout << "platform: none" << endl;
			return;
		}
		vector<string> args;
		string s;
		while (in >> s) args.push_back(s);
		uc.valid_platform = true;
		// this should be plugin-driven at some point
		if (name == "cpu") { delete rc->platform; rc->platform = new wf::cpu::platform(args); }
#ifdef HAVE_GL
		else if (name == "opengl") { delete rc->platform; rc->platform = new wf::gl::platform(args); }
#endif
#ifdef HAVE_CUDA
		else if (name == "cuda") { delete rc->platform; rc->platform = new wf::cuda::platform(args); }
#endif
		else if (name == "none") { delete rc->platform; rc->platform = nullptr; }
		else {
			delete rc->platform;
			rc->platform = nullptr;
			uc.valid_platform = false;
			error("There is no platform called '" << name << "'");
		}
		uc.tracer_touched_at = uc.cmdid;
	}
#endif
	else ifcmd("commit") {
		if (!uc.valid_platform)
			error("Invalid platform");
		if (scene.vertices.empty())
			error("There is no scene data to work with");
#ifndef RTGI_SKIP_DIRECT_ILLUM
		scene.find_light_geometry();
#endif
		if (rc->algo)
			rc->algo->data_reset_required = true;
#ifndef RTGI_SKIP_WF
		if (rc->platform)
			rc->platform->commit_scene(&scene);
		else {
#endif
			if (!scene.rt)
				error("There is no ray traversal scheme to commit the scene data to");
#ifndef RTGI_SKIP_DIRECT_ILLUM
			scene.compute_light_distribution();
#endif
			scene.rt->build(&scene);
#ifndef RTGI_SKIP_WF
		}
#endif
		uc.accel_touched_at = uc.cmdid;
	}
	else ifcmd("denoise") {
		string option;
		in >> option;
		if (option == "on") {
#ifdef HAVE_LIBOPENIMAGEDENOISE
			rc->enable_denoising = true;
#else
			std::cout << "Denoising not available: OpenImageDenoise library not found" << std::endl;
#endif
		}
		else if (option == "off") {
			rc->enable_denoising = false;
			rc->albedo_valid = false;
			rc->normal_valid = false;
		}
		else
			error("Invalid option for denoise, valid are on/off");
	}

	else ifcmd("sppx") {
		int sppx;
		in >> sppx;
		check_in_complete("Syntax error, requires exactly one positive integral value");
		rc->sppx = sppx;
	}
	else ifcmd("preview-offset") {
		int offset;
		in >> offset;
		check_in_complete("Syntax error, requires exactly one positive integral value");
		if (offset <= 0)
			error("Thge preview offset needs to be larger than zero");
		rc->preview_offset = offset;
	}
	else ifcmd("run") {
		if (!uc.valid_platform)
			error("Invalid platform");
		if (!platform_and_algo_aligned())
			error("Incompatible algorithm form platform");
		if (uc.scene_touched_at == 0 || uc.tracer_touched_at == 0 || uc.accel_touched_at == 0 || rc->algo == nullptr)
			error("We have to have a scene loaded, a ray tracer set, an acceleration structure built and an algorithm set prior to running");
		if (uc.accel_touched_at < uc.tracer_touched_at)
			error("The current tracer does (might?) not have an up-to-date acceleration structure");
		if (uc.accel_touched_at < uc.scene_touched_at)
			error("The current acceleration structure is out-dated");
#ifdef HAVE_GL
		if(preview_window)
			run_sample(rc->algo);
		else
#endif
			run(rc->algo);
	}
	else ifcmd("material") {
		string cmd;
		in >> cmd;
		if (in.eof() && cmd == "list") {
			for (auto &mtl : scene.materials) cout << mtl.name << endl;
			return;
		}
		check_in("Syntax error, requires material name, command and subsequent arguments");
		command = cmd;
		ifcmd("select") {
			string name = read_rest_of_line(in);
			material *m = nullptr; for (auto &mtl : scene.materials) if (mtl.name == name) { m = &mtl; break; }
			if (!m) error("No material called '" << name << "'");
			selected_mat = m;
			return;
		}
		ifcmd("blacklist") {
			rc->scene.mtl_blacklist.push_back(read_rest_of_line(in));
			return;
		}

		if (!selected_mat)
			error("No material selected");

		ifcmd("albedo") {
			in >> tmp;
			check_in_complete("Expects a color triplet");
			selected_mat->albedo = tmp;
		}
		else ifcmd("emissive") {
			in >> tmp;
			check_in_complete("Expects a color triplet");
			selected_mat->emissive = tmp;
		}
		else ifcmd("roughness") {
			in >> tmp.x;
			check_in_complete("Expects a floating point value");
			selected_mat->roughness = tmp.x;
		}
		else ifcmd("ior") {
			in >> tmp.x;
			check_in_complete("Expects a floating point value");
			selected_mat->ior = tmp.x;
		}
		else ifcmd("texture") {
			in >> cmd;
			check_in_complete("Expected a single (no whitespace) string value");
			// we keep the textures around as other materials might still use them.
			// they will be cleaned up by ~scene
			if (cmd == "drop")
				selected_mat->albedo_tex = nullptr;
			else {
				texture2d<vec4> *tex = load_image4f(cmd);
				if (tex)
					selected_mat->albedo_tex = tex;
			}
		}
#ifndef RTGI_SKIP_BRDF
		else ifcmd("brdf") {
			in >> cmd;
			check_in_complete("Expected a single (no whitespace) string value");
			brdf *f = nullptr;
			try {
				if (scene.brdfs.count(cmd) == 0)
					f = new_brdf(cmd, scene);
				else
					f = scene.brdfs[cmd];
				selected_mat->brdf = f;
			}
			catch (std::runtime_error &e) {
				error(e.what());
			}
		}
#endif
		else ifcmd("show") {
			check_in_complete("Does not take further arguments");
			cout << "albedo     " << selected_mat->albedo << endl;
			cout << "albedo-tex " << (selected_mat->albedo_tex ? selected_mat->albedo_tex->path.string() : string("none")) << endl;
			cout << "emissive   " << selected_mat->emissive << endl;
			cout << "roughness  " << selected_mat->roughness << endl;
			cout << "ior        " << selected_mat->ior << endl;
		}
		else error("Unknown subcommand");
	}
#ifndef RTGI_SKIP_BRDF
	else ifcmd("default-brdf") {
		string name;
		in >> name;
		check_in_complete("Expected a single (no whitespace) string value");
		if (scene.brdfs.count("default") != 0)
			cout << "Replacing default brdfs that has already been applied to some materials." << endl;
		try {
			brdf *f = new_brdf(name, scene);
			scene.brdfs["default"] = f;
		}
		catch (std::runtime_error &e) {
			error(e.what());
		}
	}
	else ifcmd("pointlight") {
		vec3 p, c;
		string cmd;
		in >> cmd;
		check_in("Command incomplete");
		bool replace = false;
		if (cmd == "replace") {
			replace = true;
			in >> cmd;
			check_in("Command incomplete");
		}
		if (cmd == "pos")
			in >> p;
		else error("Syntax error: poinlight [replace] pos x y z col x y z");
		check_in("Syntax error: poinlight [replace] pos x y z col x y z");
		in >> cmd;
		check_in("Command incomplete");
		if (cmd == "col")
			in >> c;
		else error("Syntax error: poinlight [replace] pos x y z col x y z");
		check_in_complete("Syntax error: poinlight [replace] pos x y z col x y z");
		pointlight *pl = new pointlight(p, c);
		if (replace && scene.lights.size() > 0) {
			delete scene.lights[0];
			scene.lights[0] = pl;
		}
		else
			scene.lights.push_back(pl);
	}
#endif
#ifndef RTGI_SKIP_SKY
	else ifcmd("skylight") {
		string sub;
		in >> sub;
		if (sub == "load") {
			float scale;
			string file;
			in >> file >> scale;
			check_in_complete("Syntax error: skylight load filename intensity-scale (note: filename is not allowed to contain spaces)");
			delete scene.sky;
			scene.sky = new skylight(file, scale);
		}
		else if (sub == "scale") {
			float scale;
			in >> scale;
			check_in_complete("Syntax error: skylight scale new-scale-value");
			if (!scene.sky)
				error("There is no skylight to scale");
			if (scale <= 0)
				error("Intensity scale has to be a value > 0");
			scene.sky->intensity_scale = scale;
		}
		else
			error("No such skylight subcommand");
	}
	else ifcmd("skytest") {
		string file;
		int samples;
		in >> file >> samples;
		check_in_complete("Syntax error: skytest filename samples (note: filename without spaces)");
		if (!scene.sky)
			error("No skylight defined")
		else if (!scene.sky->distribution)
			error("Skylight distribution not set up yet (use commit)")
		else {
			cout << "Running sky test..." << endl;
			scene.sky->distribution->debug_out(file, samples);
			cout << "Done." << endl;
		}
	}
#endif
#ifndef RTGI_SKIP_WF
	else ifcmd("add-scene-step") {
		string name;
		while (isspace(in.peek())) in.get();
		getline(in, name);
		check_in_complete("Syntax error: step parameters are not supported, yet");
		if (rc->platform)
			if (auto *s = rc->platform->step(name))
				rc->platform->append_setup_step(s);
			else
				error("Platform " << rc->platform->name << " does not support step " << name)
		else
			error("No platform selected, steps are available only in wavefront mode")
	}
#endif
	else ifcmd("move-speed") {
#ifdef HAVE_GL
		float speed;
		in >> speed;
		if (speed <= 0) error("Invalid speed!");
		speed_factor = speed;
#else
		error("Without GL support there is no preview");
#endif
	}
	else ifcmd("omp") {
		string sub;
		in >> sub;
		if (sub == "off")
			omp_set_num_threads(1);
		else if (sub == "on")
			omp_set_num_threads(omp_get_max_threads());
		else
			error("Syntax error: expected 'on' or 'off'");
	}
	else ifcmd("stats") {
		string sub;
		in >> sub;
		if (sub == "clear")
			stats_timer.clear();
		else if (sub == "print")
			stats_timer.print();
		else
			error("No such stats subcommand");
	}
	else ifcmd("mem-stats") {
		scene.print_memory_stats();
	}
	else ifcmd("echo") {
		string text;
		getline(in, text);
		if (text == "") text = " ";
		cout << text.substr(1) << endl;
	}
	else if (command == "") ;
	else if (command[0] == '#') ;
	else if (command.substr(0,2) == "//") ;
	else if (rc->algo && platform_and_algo_aligned() && rc->algo->interprete(command, in)) ;
	else if (scene.rt && scene.rt->interprete(command, in)) ;
	else {
		error("Unknown command");
	}
}
