#pragma once

#include <glm/glm.hpp>
#include <cmath>

#ifdef __CUDACC__
#define heterogeneous __host__ __device__
#else
#define heterogeneous
#endif

/*
 *  vector math things
 *
 */

//! compute clamped (to zero) dot product
template<typename T> heterogeneous inline float cdot(const T &a, const T &b) {
	float x = a.x*b.x + a.y*b.y + a.z*b.z;
	return x < 0.0f ? 0.0f : x;
}

//! compute absolute-value of dot product
template<typename T> heterogeneous inline float absdot(const T &a, const T &b) {
	float x = a.x*b.x + a.y*b.y + a.z*b.z;
	return x < 0.0f ? -x : x;
}


/*
 *  floating point things
 *
 */

//! move each vector component of \c from the minimal amount possible in the direction of \c to
inline vec3 nextafter(const vec3 &from, const vec3 &d) {
	return vec3(std::nextafter(from.x, d.x > 0 ? from.x+1 : from.x-1),
				std::nextafter(from.y, d.y > 0 ? from.y+1 : from.y-1),
				std::nextafter(from.z, d.z > 0 ? from.z+1 : from.z-1));
}


/*
 *  brdf helpers
 *  starting here, many things will probably originate from niho's code
 */

heterogeneous inline float clamp(float f, float min, float max) {
	return f < min ? min : (f > max ? max : f);
}

#ifndef RTGI_SKIP_LAYERED_BRDF
heterogeneous inline float fresnel_dielectric(float cos_wi, float ior_medium, float ior_material) {
#ifndef RTGI_SKIP_LAYERED_BRDF_IMPL
    // check if entering or leaving material
    const float ei = cos_wi < 0.0f ? ior_material : ior_medium;
    const float et = cos_wi < 0.0f ? ior_medium : ior_material;
    cos_wi = clamp(fabsf(cos_wi), 0.0f, 1.0f);
    // snell's law
    const float sin_t = (ei / et) * sqrtf(1.0f - cos_wi * cos_wi);
    // handle TIR
    if (sin_t >= 1.0f) return 1.0f;
	const float rev_sin2 = 1.f - sin_t * sin_t;
    const float cos_t = sqrtf(rev_sin2 > 0.0f ? rev_sin2 : 0.0f);
    const float Rparl = ((et * cos_wi) - (ei * cos_t)) / ((et * cos_wi) + (ei * cos_t));
    const float Rperp = ((ei * cos_wi) - (et * cos_t)) / ((ei * cos_wi) + (et * cos_t));
    return (Rparl * Rparl + Rperp * Rperp) / 2;
#else
    // check if entering or leaving material
    const float n1 = cos_wi < 0.0f ? ior_material : ior_medium;
    const float n2 = cos_wi < 0.0f ? ior_medium : ior_material;
    cos_wi = glm::clamp(glm::abs(cos_wi), 0.0f, 1.0f);
	// todo fresnel term for dielectrics.
	// make sure to handle internal reflection
	return 0.0f;
#endif
}
#endif


/* 
 * trigonometric helper functions
 *
 */
heterogeneous inline float cos_theta(float cos_t) {
    return cos_t;
}
heterogeneous inline float cos2_theta(float cos_t) {
    return cos_t*cos_t;
}
heterogeneous inline float abs_cos_theta(float cos_t) {
    return cos_t < 0.0f ? -cos_t : cos_t;
}
heterogeneous inline float sin2_theta(float cos_t) {
	float res = 1.0f - cos_t*cos_t;
    return res < 0.0f ? 0.0f : res;
}
heterogeneous inline float sin_theta(float cos_t) {
    return sqrtf(sin2_theta(cos_t));
}
heterogeneous inline float tan_theta(float cos_t) {
    return sin_theta(cos_t) / cos_theta(cos_t);
}
heterogeneous inline float tan2_theta(float cos_t) {
    return sin2_theta(cos_t) / cos2_theta(cos_t);
}
inline float cos_theta(const glm::vec3& N, const glm::vec3& w) {
    return glm::dot(N, w);
}
inline float abs_cos_theta(const glm::vec3& N, const glm::vec3& w) {
	float x = cos_theta(N, w);
    return x < 0.0f ? -x : x;
}
inline float cos2_theta(const glm::vec3& N, const glm::vec3& w) {
	float x = cos_theta(N, w);
    return x*x;
}
inline float sin2_theta(const glm::vec3& N, const glm::vec3& w) {
	float x = cos_theta(N, w);
	float y = 1.0f - x*x;
	return y < 0.0f ? 0.0f : y;
}
inline float sin_theta(const glm::vec3& N, const glm::vec3& w) {
    return sqrtf(sin2_theta(N, w));
}
inline float tan_theta(const glm::vec3& N, const glm::vec3& w) {
    return sin_theta(N, w) / cos_theta(N, w);
}
inline float tan2_theta(const glm::vec3& N, const glm::vec3& w) {
    return sin2_theta(N, w) / cos2_theta(N, w);
}
//! angle for given hemispherical elevation
inline float theta_z(float z) {
	return z > 1.0f ? 0.0f : acosf(z);
}
/*
 *  spherical geometry helpers
 */

template<typename T> heterogeneous inline bool same_hemisphere(const T &N, const T &v) {
    return dot(N, v) > 0;
}

inline vec2 to_spherical(const vec3 &w) {
    const float theta = theta_z(w.y);
    const float phi = atan2f(w.z, w.x);
    return vec2(glm::clamp(theta, 0.f, pi), phi < 0.f ? phi + 2.0f * pi : phi);
}

inline vec3 to_cartesian(const vec2 &w) {
    const float sin_t = sinf(w.x);
    return vec3(sin_t * cosf(w.y), sin_t * sinf(w.y), cosf(w.x));
}


/*! align vector v with given axis (e.g. to transform a tangent space sample along a world normal)
 *  \attention parameter-order inverted with regards to niho's code
 */
template<typename T> heterogeneous inline T align(const T& v, const T& axis) {
    const float s = copysign(1.f, axis.z);
    const T w = T{v.x, v.y, v.z * s};
    const T h = T{axis.x, axis.y, axis.z + s};
    const float k = dot(w, h) / (1.f + (axis.z < 0 ? -axis.z : axis.z));
    return k * h - w;
}

//! todo this might be incomplete when using geom/shading normals
template<typename T> heterogeneous inline void flip_normals_to_ray(T &normal, const T &ray_dir) {
	if (same_hemisphere(ray_dir, normal))
		normal *= -1;
}

inline void flip_normals_to_ray(diff_geom &dg, const ray &ray) {
	// todo: all computations rely on the shading normal for now,
	// this is not exactly correct.
	if (same_hemisphere(ray.d, dg.ns)) {
		dg.ng *= -1;
		dg.ns *= -1;
	}
}

template<typename T> heterogeneous inline T bary_interpol(const T &a, const T &b, const T &c, float beta, float gamma) {
	return (1-beta-gamma)*a + beta*b + gamma*c;
}

