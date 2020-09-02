#include "material.h"
#include "util.h"

using namespace glm;

#define pi float(M_PI)
#define one_over_pi (1.f / pi)
#define one_over_2pi (1.f / (2*pi))
#define one_over_4pi (1.f / (4*pi))

vec3 layered_brdf::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	const float F = fresnel_dielectric(absdot(geom.ns, w_i), 1.0f, geom.mat->ior);
	vec3 diff = base->f(geom, w_o, w_i);
	vec3 spec = coat->f(geom, w_o, w_i);
	return (1.0f-F)*diff + F*spec;
}

vec3 lambertian_reflection::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	if (!same_hemisphere(w_i, geom.ns)) return vec3(0);
	return one_over_pi * geom.albedo();
}

vec3 phong_specular_reflection::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	if (!same_hemisphere(w_i, geom.ns)) return vec3(0);
	const float exponent = exponent_from_roughness(geom.mat->roughness);
	vec3 w_h = normalize(w_o + w_i);
	const float cos_theta = cdot(w_h, geom.ns);
	const float norm_f = (exponent + 1.0f) * one_over_2pi;
	const float F = fresnel_dielectric(absdot(geom.ns, w_i), 1.0f, geom.mat->ior);
	return F * (coat ? geom.albedo() : vec3(1)) * powf(cos_theta, exponent) * norm_f;
}

