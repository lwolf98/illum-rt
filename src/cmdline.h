#pragma once

#include <string>
#include <glm/glm.hpp>

enum render_mode { simple_rt, raster };

struct cmdline {
	bool verbose = false;
	int vp_w = 128,
		vp_h = 128;
	float fovy = 65;
	float user_z = 0;
	float user_z_defined = false;
	glm::vec3 view_dir = glm::vec3(0,0,-1);
	glm::vec3 world_up = glm::vec3(0,1,0);
	glm::vec3 cam_pos  = glm::vec3(0,0,0);
	std::string outfile = "out.png";
	std::string scene = "render-data/tri.obj";
	render_mode mode;
};

extern struct cmdline cmdline;

int parse_cmdline(int argc, char **argv);
