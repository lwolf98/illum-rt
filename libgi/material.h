#pragma once

#include "rt.h"

#include <glm/glm.hpp>
#include <string>
#include <tuple>


struct texture2d;
    
inline float roughness_from_exponent(float exponent) {
	return sqrtf(2.f / (exponent + 2.f));
}
inline float exponent_from_roughness(float roughness) {
	return 2 / (roughness * roughness) - 2;
}

#ifndef RTGI_SKIP_BRDF

struct brdf {
#ifndef RTGI_SKIP_IMPORTANCE_SAMPLING
	//          w_i,f(w_i),pdf(w_i)
	typedef tuple<vec3,vec3,float> sampling_res;
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
	vec3 albedo = vec3(0);
	vec3 emissive = vec3(0);
	texture2d *albedo_tex = nullptr;
	float ior = 1.3f, roughness = 0.1f;
	struct brdf *brdf = nullptr;
};


