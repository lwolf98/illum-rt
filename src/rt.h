#pragma once

#include <glm/glm.hpp>

struct ray {
	glm::vec3 o, d;
	ray(const glm::vec3 &o, const glm::vec3 &d) : o(o), d(d) {}
	ray() {}
};
