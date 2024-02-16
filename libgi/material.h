#pragma once

#include "rt.h"

#include <glm/glm.hpp>
#include <string>
#include <tuple>

template <typename T>
struct texture2d;
    
inline float roughness_from_exponent(float exponent) {
	return sqrtf(2.f / (exponent + 2.f));
}
inline float exponent_from_roughness(float roughness) {
	return 2 / (roughness * roughness) - 2;
}

#ifndef RTGI_SKIP_BRDF

struct interaction_type {
	enum T {
		undefined = 0,
		// lobe
		diffuse  = 0b0001,
		specular = 0b0010,
		glossy   = 0b0100,
		// direction
		reflection       = 0b0'0001'0000,
		transmission_in  = 0b0'0010'0000,
		transmission_out = 0b0'0100'0000,
		// combinations
		diffuse_reflection    = diffuse  | reflection,
		specular_reflection   = specular | reflection,
		glossy_reflection     = glossy   | reflection,
		any_transmission      = transmission_out | transmission_in,
	};
	unsigned int t;
	interaction_type(unsigned int t) : t(t) {}
	friend std::ostream& operator<<(std::ostream &o, interaction_type ia) {
		bool one = false;
		#define V(X) if (ia.t & X) { if (one) o << " "; one = true; o << #X; }
		V(diffuse); V(specular); V(glossy);
		V(reflection);
		V(transmission_in); V(transmission_out);
		#undef V
		return o;
	}
};
inline bool reflection(interaction_type t) { return t.t & interaction_type::reflection; }
inline bool transmission(interaction_type t) { return t.t & interaction_type::any_transmission; }
inline bool transmission_in(interaction_type t) { return t.t & interaction_type::transmission_in; }
inline bool transmission_out(interaction_type t) { return t.t & interaction_type::transmission_out; }

struct brdf {
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	//          w_i,f(w_i),pdf(w_i)
	typedef tuple<vec3,vec3,float,interaction_type> sampling_res;
#endif
	
	virtual vec3 f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) = 0;
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	virtual float pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) = 0;
	virtual sampling_res sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) = 0;
#endif
};

//! Specular BRDFs can be layered onto non-specular ones
struct specular_brdf : public brdf {
	bool coat = false;
};

#ifndef RTGI_SKIP_LAYERED_BRDF
struct layered_brdf : public brdf {
	specular_brdf *coat;
	brdf *base;
	layered_brdf(specular_brdf *coat, brdf *base) : coat(coat), base(base) { coat->coat = true; }
	
	vec3 f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	float pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
	sampling_res sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) override;
#endif
};
#endif

struct lambertian_reflection : public brdf {
	vec3 f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	float pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
	sampling_res sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) override;
#endif
};

struct perfectly_specular_reflection : public specular_brdf {
	vec3 f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	float pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
	sampling_res sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) override;
#endif
};

#ifndef RTGI_SKIP_ASS
struct dielectric_specular_bsdf : public specular_brdf {
	vec3 f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	float pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
	sampling_res sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) override;
#endif
protected:
	sampling_res sample_r(const diff_geom &geom, const vec3 &w_o, const vec2 &xis, float R);
};

struct thin_dielectric_specular_bsdf : public dielectric_specular_bsdf {
	vec3 f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	float pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
	sampling_res sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) override;
#endif
};
#endif

struct phong_specular_reflection : public specular_brdf {
	vec3 f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	float pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
	sampling_res sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) override;
#endif
};

#ifndef RTGI_SKIP_MF_BRDF
struct gtr2_reflection : public specular_brdf {
	vec3 f(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	float pdf(const diff_geom &geom, const vec3 &w_o, const vec3 &w_i) override;
	sampling_res sample(const diff_geom &geom, const vec3 &w_o, const vec2 &xis) override;
#endif
};
#endif

brdf *new_brdf(const std::string name, scene &scene);
#endif

struct material {
	std::string name;
	vec3 albedo = vec4(0);
	vec3 emissive = vec3(0);
	texture2d<vec4> *albedo_tex = nullptr;
	float ior = 1.3f, roughness = 0.1f;
	struct brdf *brdf = nullptr;
};


