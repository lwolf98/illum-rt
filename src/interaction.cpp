#include "scene.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <glm/glm.hpp>

#include <glm/gtx/string_cast.hpp>
inline std::ostream& operator<<(std::ostream &out, const glm::vec3 &x) {
	out << to_string(x);
	return out;
}

using namespace glm;
using namespace std;

const char *prompt = "rtgi > ";

#define ifcmd(c) if (strcmp(command,c)==0)
#define error(x) { fprintf(stderr, "command %d (%s): %s\n", cmdid, command, x); continue; }

void repl(FILE *in, scene &scene) {
	int cmdid = 0;
	while (!feof(in)) {
		cmdid++;
		if (in == stdin) {
			fprintf(stdout, "%s", prompt);
			fflush(stdout);
		}
		char *command;
		fscanf(in, "%ms", &command);

		ifcmd("at") {
			if (fscanf(in, "%f %f %f", &scene.camera.pos.x, &scene.camera.pos.y, &scene.camera.pos.z) != 3)
				error("Syntax error, requires 3 numerical components");
		}
		else ifcmd("look") {
			if (fscanf(in, "%f %f %f", &scene.camera.dir.x, &scene.camera.dir.y, &scene.camera.dir.z) != 3)
				error("Syntax error, requires 3 numerical components");
		}
		else ifcmd("up") {
			if (fscanf(in, "%f %f %f", &scene.up.x, &scene.up.y, &scene.up.z) != 3)
				error("Syntax error, requires 3 numerical components");
		}
		else ifcmd("bookmark") {
			
		}

		free(command);
	}
}
