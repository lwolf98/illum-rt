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
inline std::ostream& operator<<(std::ostream &out, const glm::vec3 &x) {
	out << to_string(x);
	return out;
}

inline std::istream& operator>>(std::istream &in, glm::vec3 &x) {
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
		else if (command == "") ;
		else if (algo && algo->interprete(command, in)) ;
		else {
			error("Unknown command");
		}
	}
	cout << endl;

	delete algo;
}
