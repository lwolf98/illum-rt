#pragma once

#include "cmdline.h"
#include "camera.h"

#include <vector>
#include <map>
#include <string>

struct vertex {
	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 tc;
};

struct triangle {
	uint32_t a, b, c;
	uint32_t material_id;
};

struct material {
	std::string name;
	glm::vec4 kd, ks;
};

struct scene {
	std::vector<::vertex>   vertices;
	std::vector<::triangle> triangles;
	std::vector<::material> materials;
	std::map<std::string, ::camera> cameras;
	::camera camera;
	glm::vec3 up;
	const ::material& material(uint32_t triangle_index) const {
		return materials[triangles[triangle_index].material_id];
	}
	scene() : camera(glm::vec3(0,0,-1), glm::vec3(0,0,0), glm::vec3(0,1,0), 65, 1, 1) {
	}
};

// std::vector<triangle> scene_triangles();
scene load_scene(const std::string& filename);
