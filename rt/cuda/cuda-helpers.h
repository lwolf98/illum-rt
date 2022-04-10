#pragma once

#include <iostream>
#include <cuda.h>
#include <cuda_runtime.h>

#define CHECK_CUDA_ERROR(f,str) {cudaError_t err = (f); if (err) {wf::cuda::check_cuda_error(err, __FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(#f)+": "+str); }}

namespace wf {
    namespace cuda {
        void check_cuda_error(cudaError_t err, const char* const file, int const line, char const* const func, const std::string &f);
    }
}
