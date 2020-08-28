#define pi float(M_PI)
#define one_over_pi (1.f / pi)
#define one_over_2pi (1.f / (2*pi))
#define one_over_4pi (1.f / (4*pi))

inline bool same_hemisphere(const glm::vec3 &N, const glm::vec3 &v) {
    return glm::dot(N, v) > 0;
}

inline float cdot(const glm::vec3 &a, const glm::vec3 &b) {
	float x = a.x*b.x + a.y*b.y + a.z*b.z;
	return x < 0.0f ? 0.0f : x;
}
