#pragma once

#include "rt.h"

#include <glm/glm.hpp>
#include <string>

struct texture;

struct material {
	std::string name;
	glm::vec3 albedo = glm::vec3(0);
	texture *albedo_tex = nullptr;
};

struct brdf {
	virtual glm::vec3 f(const diff_geom &geom, const glm::vec3 &wo, const glm::vec3 &wi) = 0;
};

struct lambertian_reflection : public brdf {
	glm::vec3 f(const diff_geom &geom, const glm::vec3 &wo, const glm::vec3 &wi) override;
};
