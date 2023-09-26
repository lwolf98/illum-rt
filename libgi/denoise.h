#include "config.h"

#ifdef HAVE_LIBOPENIMAGEDENOISE
#include "OpenImageDenoise/oidn.hpp"
#endif

#include "glm/glm.hpp"

#include <chrono>
#include <iostream>

void denoise(size_t width, size_t height, glm::vec4 *color);

// Albedo/Normalbuffers that dont exist can just be nullptr.
void denoise(size_t width, size_t height, glm::vec4 *color, glm::vec4 *albedo, glm::vec4 *normal, bool prefilter_auxiliary);
