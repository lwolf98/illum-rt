#include "optix-helper.h"
#include <iostream>
#include <optix_stubs.h>

namespace wf::cuda {
    void check_optix_error(OptixResult result, const std::string &file, const int line, const std::string &func, const std::string &f) {
        std::cerr << "Optix Error: in file " << file << ":" << line << std::endl;
        std::cerr << "Optix Error: in function " << func << std::endl;
        std::cerr << "Optix Error: from " << f << std::endl;
        std::cerr << "Optix Error: " << optixGetErrorString(result) << " (" << optixGetErrorName(result) << ")" << std::endl;
        throw std::runtime_error("OptiX Error");
    }
}