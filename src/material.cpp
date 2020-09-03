#include "material.h"
#include "util.h"
#include "sampling.h"

using namespace glm;

// vec3 layered_brdf::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
// 	const float F = fresnel_dielectric(absdot(geom.ns, w_i), 1.0f, geom.mat->ior);
// 	vec3 diff = base->f(geom, w_o, w_i);
// 	vec3 spec = coat->f(geom, w_o, w_i);
// 	return (1.0f-F)*diff + F*spec;
// }

// lambertian_reflection

vec3 lambertian_reflection::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	if (!same_hemisphere(w_i, geom.ns)) return vec3(0);
	return one_over_pi * geom.albedo();
}

float lambertian_reflection::pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
    return absdot(geom.ns, w_i) * one_over_pi;
}

brdf::sampling_res lambertian_reflection::sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) {
	vec3 w_i = align(cosine_sample_hemisphere(xis), geom.ns);
	if (!same_hemisphere(w_i, geom.ng)) return { w_i, vec3(0), 0 };
	float pdf_val = pdf(geom, w_o, w_i);
    assert(std::isfinite(pdf_val));
	return { w_i, f(geom, w_o, w_i), pdf_val };
}

// specular_reflection

vec3 phong_specular_reflection::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	if (!same_hemisphere(w_i, geom.ns)) return vec3(0);
	const float exponent = exponent_from_roughness(geom.mat->roughness);
	vec3 w_h = normalize(w_o + w_i);
	const float cos_theta = cdot(w_h, geom.ns);
	const float norm_f = (exponent + 1.0f) * one_over_2pi;
	//const float F = fresnel_dielectric(absdot(geom.ns, w_i), 1.0f, geom.mat->ior);
	return (coat ? vec3(1) : geom.albedo()) * powf(cos_theta, exponent) * norm_f;
}

float phong_specular_reflection::pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	float exp = exponent_from_roughness(geom.mat->roughness);
	vec3 r = 2.0f*geom.ns*dot(geom.ns,w_o) - w_o;
	float z = cdot(r,w_i);
	return powf(z, exp) * (exp+1.0f) * one_over_2pi;
}

brdf::sampling_res phong_specular_reflection::sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) {
	float exp = exponent_from_roughness(geom.mat->roughness);
	float z = powf(xis.x, 1.0f/exp);
	float phi = 2.0f*pi*xis.y;
	vec3 sample(sqrtf(1.0f-z*z) * cos(phi),
				sqrtf(1.0f-z*z) * sin(phi),
				z);
	vec3 r = 2.0f*geom.ns*dot(geom.ns,w_o) - w_o;
	vec3 w_i = align(sample, r);
	if (!same_hemisphere(w_i, geom.ng)) return { w_i, vec3(0), 0 };
	float pdf_val = powf(z,exp) * (exp+1.0f) * one_over_2pi;
// 	float comp_pdf = pdf(geom, w_o, w_i);
	return { w_i, f(geom, w_o, w_i), pdf_val };
}
