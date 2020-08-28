#include "material.h"
#include "util.h"

using namespace glm;

#define pi float(M_PI)
#define one_over_pi (1.f / pi)
#define one_over_2pi (1.f / (2*pi))
#define one_over_4pi (1.f / (4*pi))

glm::vec3 lambertian_reflection::f(const diff_geom &geom, const glm::vec3 &wo, const glm::vec3 &wi) {
	if (!same_hemisphere(wo, geom.ns)) return vec3(0);
	if (!same_hemisphere(wi, geom.ns)) return vec3(0);
	return one_over_pi * geom.albedo();
}

