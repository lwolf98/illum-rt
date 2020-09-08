#pragma once

inline float luma(const vec3& rgb) {
    return glm::dot(vec3(0.212671f, 0.715160f, 0.072169f), rgb);
}

