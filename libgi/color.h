#pragma once

//! Perceptional brightness of a color
inline float luma(const vec3& rgb) {
    return glm::dot(vec3(0.212671f, 0.715160f, 0.072169f), rgb);
}

// heatmap (blue to red) from given value in [0, 1], taken from niho's code
inline glm::vec3 heatmap(float val) {
    const float hue = 251.1 / 360.f; // blue
    const glm::vec3 hsv = glm::vec3(hue + glm::clamp(val, 0.f, 1.f) * -hue, 1, val < 1e-4f ? 0 : 1); // from blue to red
    // map hsv to rgb
    const glm::vec4 K = glm::vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    const glm::vec3 p = glm::abs(glm::fract(glm::vec3(hsv.x) + glm::vec3(K)) * 6.f - glm::vec3(K.w));
    return hsv.z * glm::mix(glm::vec3(K.x), clamp(p - glm::vec3(K.x), glm::vec3(0.f), glm::vec3(1.f)), hsv.y);
}
