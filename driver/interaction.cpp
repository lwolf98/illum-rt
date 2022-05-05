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
#include "libgi/wavefront-rt.h"

#include "rt/cpu/platform.h"

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
#include "rt/gl/platform.h"
#endif

#ifdef HAVE_CUDA
#include "rt/cuda/platform.h"
#endif

#include "libgi/timer.h"

#include "libgi/global-context.h"

#include <iostream>
#include <sstream>

#include <glm/glm.hpp>
#if GLM_VERSION < 997
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/string_cast.hpp>

inline std::istream& operator>>(std::istream &in, vec3 &x) {
	in >> x.x >> x.y >> x.z;
	return in;
}

using namespace glm;
using namespace std;

const char *prompt = "rtgi > ";

#define ifcmd(c) if (command==c)
#define error_no_continue(x) { cerr << "command " << uc.cmdid << " (" << command << "): " << x << endl; }
#define error(x) { error_no_continue(x);  continue; }
#define check_in(x) { if (in.bad() || in.fail()) error(x); }
#define check_in_complete(x) { if (in.bad() || in.fail() || !in.eof()) error(x); }

void run(gi_algorithm *algo);
void rt_bench();

void repl(istream &infile, repl_update_checks &uc) {
	bool cam_has_pos = false,
		 cam_has_dir = false,
		 cam_has_up = false,
		 scene_up_set = false;

	gi_algorithm *&algo = rc->algo;
	scene &scene = rc->scene;
	framebuffer &framebuffer = rc->framebuffer;

	material *mat = nullptr;
	vector<string> commands;

	while (!infile.eof()) {
		if (&infile == &cin)
			cout << prompt << flush;
		uc.cmdid++;
		string line, command;
		getline(infile, line);
		commands.push_back(line);
		istringstream in(line);

		in >> command;
		vec3 tmp;
		ifcmd("history") {
			commands.pop_back();
			for (auto &x : commands)
				cout << x << endl;
		}
		else ifcmd("quit")
			return;
		else ifcmd("exit")
			return;
		else ifcmd("at") {
			in.get();
			if (in.eof()) {
				cout << scene.camera.pos << endl;
				continue;
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
				continue;
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
				continue;
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
				continue;
			}
			in >> tmp;
			check_in_complete("Syntax error, requires 3 numerical components");
			scene.camera.up = tmp;
			cam_has_up = true;
		}
		else ifcmd("load") {
			string file, name;
			in >> file;
			if (!in.eof())
				in >> name;
			check_in_complete("Syntax error, requires a file name (no spaces, sorry) and (optionally) a name");
			scene.add(file, name);
			uc.scene_touched_at = uc.cmdid;
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
#ifndef RTGI_SKIP_WF
			else if (name == "primary-wf")  a = new wf::primary_hit_display;
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
				delete algo;
				algo = a;
			}
		}
		else ifcmd("outfile") {
			string name;
			in >> name;
			check_in_complete("Syntax error, only accepts a single file name (no spaces, sorry)");
			cmdline.outfile = name;
		}
// 		else ifcmd("bookmark") {
// 			
// 		}
		else ifcmd("raytracer") {
			if (rc->platform) {
				rc->platform->interprete(command, in);
				continue;
			}
			string name;
			in >> name;
			if (name == "seq") scene.use(new seq_tri_is);
#ifndef RTGI_SKIP_BVH
			else if (name == "naive-bvh") scene.use(new naive_bvh);
			else if (name == "bbvh") {
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
			}
#endif
			else error("There is no ray tracer called '" << name << "'");
			uc.tracer_touched_at = uc.cmdid;
		}
		else ifcmd("platform") {
			string name;
			in >> name;
			check_in("Syntax error, requires platform name");
			vector<string> args;
			string s;
			while (in >> s) args.push_back(s);
			// this should be plugin-driven at some point
			if (name == "cpu") rc->platform = new wf::cpu::platform(args);
#ifdef HAVE_GL
			else if (name == "opengl") rc->platform = new wf::gl::platform(args);
#endif
#ifdef HAVE_CUDA
			else if (name == "cuda") rc->platform = new wf::cuda::platform(args);
#endif
			else error("There is no platform called '" << name << "'");
			uc.tracer_touched_at = uc.cmdid;
		}
		else ifcmd("commit") {
			if (scene.vertices.empty())
				error("There is no scene data to work with");
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
		else ifcmd("sppx") {
			int sppx;
			in >> sppx;
			check_in_complete("Syntax error, requires exactly one positive integral value");
			rc->sppx = sppx;
		}
		else ifcmd("run") {
			if (uc.scene_touched_at == 0 || uc.tracer_touched_at == 0 || uc.accel_touched_at == 0 || algo == nullptr)
				error("We have to have a scene loaded, a ray tracer set, an acceleration structure built and an algorithm set prior to running");
			if (uc.accel_touched_at < uc.tracer_touched_at)
				error("The current tracer does (might?) not have an up-to-date acceleration structure");
			if (uc.accel_touched_at < uc.scene_touched_at)
				error("The current acceleration structure is out-dated");
			run(algo);
		}
		else ifcmd("rt_bench") {
#ifndef WITH_STATS		
			if (uc.scene_touched_at == 0 || uc.tracer_touched_at == 0 || uc.accel_touched_at == 0)
				error("We have to have a scene loaded, a ray tracer set, an acceleration structure built prior to running");
			if (uc.accel_touched_at < uc.tracer_touched_at)
				error("The current tracer does (might?) not have an up-to-date acceleration structure");
			if (uc.accel_touched_at < uc.scene_touched_at)
				error("The current acceleration structure is out-dated");
			rt_bench(rc);
#else
			cerr << "ERROR: cannot run rt-bench when WITH_STATS is defined" << endl;
#endif
		}
		else ifcmd("mesh") {
			string name, cmd;
			in >> name;
			if (in.eof() && name == "list") {
				for (auto &obj : scene.objects) cout << obj.name << endl;
				continue;
			}
			error("Meshes can only be listed in this version");
		}
		else ifcmd("material") {
			string cmd;
			in >> cmd;
			if (in.eof() && cmd == "list") {
				for (auto &mtl : scene.materials) cout << mtl.name << endl;
				continue;
			}
			check_in("Syntax error, requires material name, command and subsequent arguments");
			command = cmd;
			ifcmd("select") {
				string name;
				in >> name;
				check_in_complete("Only a single string (no whitespace) accepted");
				material *m = nullptr; for (auto &mtl : scene.materials) if (mtl.name == name) { m = &mtl; break; }
				if (!m) error("No material called '" << name << "'");
				mat = m;
				continue;
			}
			if (!mat)
				error("No material selected");
			ifcmd("albedo") {
				in >> tmp;
				check_in_complete("Expects a color triplet");
				mat->albedo = tmp;
			}
			else ifcmd("emissive") {
				in >> tmp;
				check_in_complete("Expects a color triplet");
				mat->emissive = tmp;
			}
			else ifcmd("roughness") {
				in >> tmp.x;
				check_in_complete("Expects a floating point value");
				mat->roughness = tmp.x;
			}
			else ifcmd("ior") {
				in >> tmp.x;
				check_in_complete("Expects a floating point value");
				mat->ior = tmp.x;
			}
			else ifcmd("texture") {
				in >> cmd;
				check_in_complete("Expected a single (no whitespace) string value");
				// we keep the textures around as other materials might still use them.
				// they will be cleaned up by ~scene
				if (cmd == "drop")
					mat->albedo_tex = nullptr;
				else {
					texture2d *tex = load_image3f(cmd);
					if (tex)
						mat->albedo_tex = tex;
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
					mat->brdf = f;
				}
				catch (std::runtime_error &e) {
					error(e.what());
				}
			}
#endif
			else ifcmd("show") {
				check_in_complete("Does not take further arguments");
				cout << "albedo     " << mat->albedo << endl;
				cout << "albedo-tex " << (mat->albedo_tex ? mat->albedo_tex->path.string() : string("none")) << endl;
				cout << "emissive   " << mat->emissive << endl;
				cout << "roughness  " << mat->roughness << endl;
				cout << "ior        " << mat->ior << endl;
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
		else ifcmd("echo") {
			string text;
			char c; in >> c; // skip first whitespace
			getline(in, text);
			cout << text << endl;
		}
		else if (command == "") ;
		else if (command[0] == '#') ;
		else if (algo && algo->interprete(command, in)) ;
		else if (scene.rt && scene.rt->interprete(command, in)) ;
		else {
			error("Unknown command");
		}
	}
	cout << endl;
}
