#pragma once

// in here, many things will probably originate from niho's code


inline vec2 uniform_sample_disk(const vec2 &sample) {
    const float r = sqrtf(sample.x);
    const float theta = 2 * pi * sample.y;
    return r * vec2(cosf(theta), sinf(theta));
}

// hemisphere -> uniformly distributed tangent space direction
inline vec3 uniform_sample_hemisphere(const vec2 &sample) {
    const float z = sample.x;
	const float r0 = 1 - z * z;
    const float r = sqrtf(r0 < 0 ? -r0 : r0);
    const float phi = 2 * pi * sample.y;
    return glm::vec3(r * cosf(phi), r * sinf(phi), z);
}
inline float uniform_hemisphere_pdf() {
    return one_over_2pi;
}

// hemisphere -> cosine distributed tangent space direction
inline vec3 cosine_sample_hemisphere(const vec2 &sample) {
    const vec2 d = uniform_sample_disk(sample);
    const float z = 1 - d.x*d.x - d.y*d.y;
    return glm::vec3(d.x, d.y, z > 0 ? sqrtf(z) : 0.f);
}
inline float cosine_hemisphere_pdf(float cos_t) {
    return cos_t / pi;
}

// triangles (returns baryzentric coordinates)
inline vec2 uniform_sample_triangle(const glm::vec2 &sample) {
    const float su0 = sqrtf(sample.x);
    return glm::vec2(1.0f - su0, sample.y * su0);
}


