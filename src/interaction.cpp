#include "scene.h"
#include "algorithm.h"
#include "bvh.h"
#include "framebuffer.h"
#include "context.h"

#include "primary-hit.h"

#include <iostream>
#include <sstream>
#include <glm/glm.hpp>

#include <glm/gtx/string_cast.hpp>
inline std::ostream& operator<<(std::ostream &out, const vec3 &x) {
	out << to_string(x);
	return out;
}

inline std::istream& operator>>(std::istream &in, vec3 &x) {
	in >> x.x >> x.y >> x.z;
	return in;
}

using namespace glm;
using namespace std;

const char *prompt = "rtgi > ";

#define ifcmd(c) if (command==c)
#define error(x) { cerr << "command " << cmdid << " (" << command << "): " << x << endl; continue; }
#define check_in(x) { if (in.bad() || in.fail()) error(x); }
#define check_in_complete(x) { if (in.bad() || in.fail() || !in.eof()) error(x); }

void run(render_context &rc, gi_algorithm *algo);

void repl(istream &infile, render_context &rc) {
	int cmdid = 0;
	bool cam_has_pos = false,
		 cam_has_dir = false,
		 cam_has_up = false,
		 scene_up_set = false;

	gi_algorithm *algo = nullptr;
	scene &scene = rc.scene;
	framebuffer &framebuffer = rc.framebuffer;

	unsigned scene_touched_at = 0,
			 tracer_touched_at = 0,
			 accel_touched_at = 0;

	while (!infile.eof()) {
		if (&infile == &cin)
			cout << prompt << flush;
		cmdid++;
		string line, command;
		getline(infile, line);
		istringstream in(line);

		in >> command;
		vec3 tmp;
		ifcmd("at") {
			in >> tmp;
			check_in_complete("Syntax error, requires 3 numerical components");
			scene.camera.pos = tmp;
			cam_has_pos = true;
		}
		else ifcmd("look") {
			in >> tmp;
			check_in_complete("Syntax error, requires 3 numerical components");
			scene.camera.dir = tmp;
			cam_has_dir = true;
		}
		else ifcmd("up") {
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
			scene_touched_at = cmdid;
		}
		else ifcmd("resolution") {
			int w, h;
			in >> w >> h;
			check_in_complete("Syntax error, requires 2 integral values");
			framebuffer.resize(w, h);
			scene.camera.update_frustum(scene.camera.fovy, w, h);
		}
		else ifcmd("algo") {
			string name;
			in >> name;
			if (name == "primary")	algo = new primary_hit_display;
			else error("There is no gi algorithm called '" << name << "'");
		}
// 		else ifcmd("bookmark") {
// 			
// 		}
		else ifcmd("raytracer") {
			string name;
			in >> name;
			if (name == "bbvh") scene.rt = new binary_bvh_tracer;
			else error("There is no ray tracer called '" << name << "'");
			tracer_touched_at = cmdid;
		}
		else ifcmd("commit") {
			if (scene.vertices.empty())
				error("There is no scene data to work with");
			if (!scene.rt)
				error("There is no ray traversal scheme to commit the scene data to");
			scene.compute_light_distribution();
			scene.rt->build(&scene);
			accel_touched_at = cmdid;
		}
		else ifcmd("sppx") {
			int sppx;
			in >> sppx;
			check_in_complete("Syntax error, requires exactly one positive integral value");
			rc.sppx = sppx;
		}
		else ifcmd("run") {
			if (scene_touched_at == 0 || tracer_touched_at == 0 || accel_touched_at == 0 || algo == nullptr)
				error("We have to have a scene loaded, a ray tracer set, an acceleration structure built and an algorithm set prior to running");
			if (accel_touched_at < tracer_touched_at)
				error("The current tracer does (might?) not have an up-to-date acceleration structure");
			if (accel_touched_at < scene_touched_at)
				error("The current acceleration structure is out-dated");
			run(rc, algo);
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
			string name, cmd;
			in >> name;
			if (in.eof() && name == "list") {
				for (auto &mtl : scene.materials) cout << mtl.name << endl;
				continue;
			}
			check_in("Syntax error, requires material name, command and subsequent arguments");
			material *m = nullptr;
			for (auto &mtl : scene.materials) if (mtl.name == name) { m = &mtl; break; }
			if (!m) error("No material called '" << name << "'");
			command = cmd;
			ifcmd("albedo") {
			}
			else ifcmd("emissive") {
			}
			else ifcmd("set") {
			}
			else error("Unknown subcommand");
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
		else if (command == "") ;
		else if (command[0] == '#') ;
		else if (algo && algo->interprete(command, in)) ;
		else {
			error("Unknown command");
		}
	}
	cout << endl;

	delete algo;
}
