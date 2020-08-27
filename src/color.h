#pragma once

inline float luma(const glm::vec3& rgb) {
    return glm::dot(glm::vec3(0.212671f, 0.715160f, 0.072169f), rgb);
}

