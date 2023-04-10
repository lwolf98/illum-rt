#pragma once

#ifdef __CUDACC__
#define heterogeneous __host__ __device__
#else
#define heterogeneous
#endif


// in here, many things will probably originate from niho's code


template<typename T> heterogeneous inline T uniform_sample_disk(const T &sample) {
    const float r = sqrtf(sample.x);
    const float theta = 2 * pi * sample.y;
    return r * T{cosf(theta), sinf(theta)};
}

// sphere -> uniformly distributed tangent space direction
// pbrt3/776
template<typename vec3=glm::vec3, typename vec2=glm::vec2> heterogeneous inline vec3 uniform_sample_sphere(const vec2 &sample) {
	float z = 1.f - 2.f * sample.x;
	float r_tmp = 1.f - z*z;
	float r = sqrtf(r_tmp > 0.f ? r_tmp : 0.f);
	float phi = 2.f * pi * sample.y;
	return vec3(r*cosf(phi), r*sinf(phi), z);
}
inline float uniform_sphere_pdf() {
	return one_over_4pi;
}

// hemisphere -> uniformly distributed tangent space direction
template<typename vec3=glm::vec3, typename vec2=glm::vec2> heterogeneous inline vec3 uniform_sample_hemisphere(const vec2 &sample) {
    const float z = sample.x;
	const float r0 = 1 - z * z;
    const float r = sqrtf(r0 < 0 ? -r0 : r0);
    const float phi = 2 * pi * sample.y;
    return vec3{r * cosf(phi), r * sinf(phi), z};
}
inline float uniform_hemisphere_pdf() {
    return one_over_2pi;
}

// hemisphere -> cosine distributed tangent space direction
template<typename vec3=glm::vec3, typename vec2=glm::vec2> heterogeneous inline vec3 cosine_sample_hemisphere(const vec2 &sample) {
    const vec2 d = uniform_sample_disk(sample);
    const float z = 1 - d.x*d.x - d.y*d.y;
    return vec3{d.x, d.y, z > 0 ? sqrtf(z) : 0.f};
}
inline float cosine_hemisphere_pdf(float cos_t) {
    return cos_t / pi;
}

// triangles (returns baryzentric coordinates)
template<typename vec2=glm::vec2> heterogeneous inline vec2 uniform_sample_triangle(const vec2 &sample) {
    const float su0 = sqrtf(sample.x);
    return vec2{1.0f - su0, sample.y * su0};
}


