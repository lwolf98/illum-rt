#pragma once

#include <glm/glm.hpp>
#include <cmath>

#define pi float(M_PI)
#define one_over_pi (1.f / pi)
#define one_over_2pi (1.f / (2*pi))
#define one_over_4pi (1.f / (4*pi))

inline bool same_hemisphere(const glm::vec3 &N, const glm::vec3 &v) {
    return glm::dot(N, v) > 0;
}

//! compute clamped (to zero) dot product
inline float cdot(const glm::vec3 &a, const glm::vec3 &b) {
	float x = a.x*b.x + a.y*b.y + a.z*b.z;
	return x < 0.0f ? 0.0f : x;
}

//! move each vector component of \c from the minimal amount possible in the direction of \c to
inline glm::vec3 nextafter(const glm::vec3 &from, const glm::vec3 &d) {
	return glm::vec3(std::nextafter(from.x, d.x > 0 ? from.x+1 : from.x-1),
					 std::nextafter(from.y, d.y > 0 ? from.y+1 : from.y-1),
					 std::nextafter(from.z, d.z > 0 ? from.z+1 : from.z-1));
}
