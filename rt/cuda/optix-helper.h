#pragma once

#include <optix.h>
#include <string>

#define CHECK_OPTIX_ERROR(f,str) {OptixResult result = (f); if (result != OPTIX_SUCCESS) {wf::cuda::check_optix_error(result, __FILE__, __LINE__, __PRETTY_FUNCTION__, #f ": " #str); }}

namespace wf::cuda {
    void check_optix_error(OptixResult result, const std::string &file, int const line, const std::string &func, const std::string &f);
}
