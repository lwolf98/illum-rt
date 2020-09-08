#pragma once

#include <string>
#include <glm/glm.hpp>

enum render_mode { simple_rt, raster };

struct cmdline {
	bool verbose = false;
	std::string script;
	bool interact = true;
};

extern struct cmdline cmdline;

int parse_cmdline(int argc, char **argv);
