#include "material.h"
#include "scene.h"
#include "util.h"
#ifndef RTGI_AXX
#include "sampling.h"
#endif

using namespace glm;

#ifndef RTGI_A03
vec3 layered_brdf::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
#ifndef RTGI_AXX
	const float F = fresnel_dielectric(absdot(geom.ns, w_o), 1.0f, geom.mat->ior);
	vec3 diff = base->f(geom, w_o, w_i);
	vec3 spec = coat->f(geom, w_o, w_i);
	return (1.0f-F)*diff + F*spec;
#else
	vec3 diff = base->f(geom, w_o, w_i);
	vec3 spec = coat->f(geom, w_o, w_i);
	return diff+spec;
#endif
}
#endif

#ifndef RTGI_AXX
float layered_brdf::pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	const float F = fresnel_dielectric(absdot(geom.ns, w_o), 1.0f, geom.mat->ior);
	float pdf_diff = base->pdf(geom, w_o, w_i);
	float pdf_spec = coat->pdf(geom, w_o, w_i);
	return (1.0f-F)*pdf_diff + F*pdf_spec;
}

brdf::sampling_res layered_brdf::sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) {
	const float F = fresnel_dielectric(absdot(geom.ns, w_o), 1.0f, geom.mat->ior);
	if (xis.x < F) {
		// specular sample
		vec2 new_xi((F-xis.x)/F, xis.y);
		auto [w_i, f_spec, pdf_spec] = coat->sample(geom, w_o, new_xi);
		vec3 f_diff = base->f(geom, w_o, w_i);
		float pdf_diff = base->pdf(geom, w_o, w_i);
		return { w_i, (1.0f-F)*f_diff + F*f_spec, (1.0f-F)*pdf_diff + F*pdf_spec };
	}
	else {
		vec2 new_xi((xis.x-F)/(1.0f-F), xis.y);
		auto [w_i, f_diff, pdf_diff] = base->sample(geom, w_o, new_xi);
		vec3 f_spec = coat->f(geom, w_o, w_i);
		float pdf_spec = coat->pdf(geom, w_o, w_i);
		return { w_i, (1.0f-F)*f_diff + F*f_spec, (1.0f-F)*pdf_diff + F*pdf_spec };
	}
}
#endif

// lambertian_reflection

vec3 lambertian_reflection::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	if (!same_hemisphere(w_i, geom.ns)) return vec3(0);
#ifndef RTGI_A03
	return one_over_pi * geom.albedo();
#else
	// todo
	return vec3(0);
#endif
}

#ifndef RTGI_AXX
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
#endif

// specular_reflection

vec3 phong_specular_reflection::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	if (!same_hemisphere(w_i, geom.ng)) return vec3(0);
#ifndef RTGI_A03
	float exponent = exponent_from_roughness(geom.mat->roughness);
	vec3 r = 2.0f*geom.ns*dot(w_i,geom.ns)-w_i;
	float cos_theta = cdot(w_o, r);
	const float norm_f = (exponent + 2.0f) * one_over_2pi;
	return (coat ? vec3(1) : geom.albedo()) * powf(cos_theta, exponent) * norm_f * cdot(w_i,geom.ns);
#else
	// todo
	return vec3(0);
#endif
}

#ifndef RTGI_AXX
float phong_specular_reflection::pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
	float exp = exponent_from_roughness(geom.mat->roughness);
	vec3 r = 2.0f*geom.ns*dot(geom.ns,w_o) - w_o;
	float z = cdot(r,w_i);
	return z* (exp+1.0f) * one_over_2pi;
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
	float pdf_val = z * (exp+1.0f) * one_over_2pi;
	return { w_i, f(geom, w_o, w_i), pdf_val };
}
#endif


#ifndef RTGI_AXX

// Microfacet distribution helper functions
#define sqr(x) ((x)*(x))

inline float ggx_d(const float NdotH, float roughness) {
    if (NdotH <= 0) return 0.f;
    const float tan2 = tan2_theta(NdotH);
    if (!std::isfinite(tan2)) return 0.f;
    const float a2 = sqr(roughness);
    return a2 / (pi * sqr(sqr(NdotH)) * sqr(a2 + tan2));
}

inline float ggx_g1(const float NdotV, float roughness) {
    if (NdotV <= 0) return 0.f;
    const float tan2 = tan2_theta(NdotV);
    if (!std::isfinite(tan2)) return 0.f;
    return 2.f / (1.f + sqrtf(1.f + sqr(roughness) * tan2));
}

vec3 ggx_sample(const vec2 &xi, float roughness) {
    const float theta = atanf((roughness * sqrtf(xi.x)) / sqrtf(1.f - xi.x));
    if (!std::isfinite(theta)) return vec3(0, 0, 1);
    const float phi = 2.f * pi * xi.y;
    const float sin_t = sinf(theta);
    return vec3(sin_t * cosf(phi), sin_t * sinf(phi), cosf(theta));
}

inline float ggx_pdf(float D, float NdotH, float HdotV) {
    return (D * (NdotH<0 ? -NdotH : NdotH)) / (4.f * (HdotV<0 ? -HdotV : HdotV));
}
#undef sqr

vec3 gtr2_reflection::f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
    if (!same_hemisphere(geom.ng, w_i)) return vec3(0);
    const float NdotV = dot(geom.ns, w_o);
    const float NdotL = dot(geom.ns, w_i);
    if (NdotV == 0.f || NdotV == 0.f) return vec3(0);
    const vec3 H = normalize(w_o + w_i);
    const float NdotH = dot(geom.ns, H);
    const float HdotL = dot(H, w_i);
    const float roughness = geom.mat->roughness;
    const float F = fresnel_dielectric(HdotL, 1.f, geom.mat->ior);
    const float D = ggx_d(NdotH, roughness);
    const float G = ggx_g1(NdotV, roughness) * ggx_g1(NdotL, roughness);
    const float microfacet = (F * D * G) / (4 * abs(NdotV) * abs(NdotL));
    return coat ? vec3(microfacet) : 15.f*geom.albedo() * microfacet;
}

#ifndef RTGI_AXX
float gtr2_reflection::pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) {
    const vec3 H = normalize(w_o + w_i);
    const float NdotH = dot(geom.ns, H);
    const float HdotV = dot(H, w_o);
    const float D = ggx_d(NdotH, geom.mat->roughness);
    return ggx_pdf(D, NdotH, HdotV);
}

brdf::sampling_res gtr2_reflection::sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) {
	// reflect around sampled macro normal w_h
	const vec3 w_h = align(ggx_sample(xis, geom.mat->roughness), geom.ns);
	vec3 w_i = 2.0f*w_h*dot(w_h, w_o) - w_o;
	if (!same_hemisphere(geom.ng, w_i)) return { w_i, vec3(0), 0 };
	float sample_pdf = pdf(geom, w_o, w_i);
	assert(sample_pdf > 0);
	assert(std::isfinite(sample_pdf));
	return { w_i, f(geom, w_o, w_i), sample_pdf };
}
#endif

#endif

brdf *new_brdf(const std::string name, scene &scene) {
	if (scene.brdfs.count(name) == 0) {
		brdf *f = nullptr;
		if (name == "lambert")
			f = new lambertian_reflection;
		else if (name == "phong")
			f = new phong_specular_reflection;
		else if (name == "layered-phong") {
#ifndef RTGI_A03
			brdf *base = new_brdf("lambert", scene);
			specular_brdf *coat = dynamic_cast<specular_brdf*>(new_brdf("phong", scene));
			assert(coat);
			f = new layered_brdf(coat, base);
#else
			throw std::logic_error("Not implemented, yet");
#endif
		}
#ifndef RTGI_AXX
		else if (name == "gtr2")
			f = new gtr2_reflection;
#endif
		else
			throw std::runtime_error(std::string("No such brdf defined: ") + name);
		return f;
	}
	return scene.brdfs[name];
}
