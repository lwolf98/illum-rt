#pragma once

#include <iostream>
#include<cuda.h>
#include <cuda_runtime.h>

#define CHECK_CUDA_ERROR(f) {cudaError_t err = (f); if(err){wf::cuda::checkCudaError(err, __FILE__, __LINE__, __func__, #f); }}

namespace wf{
    namespace cuda{
        void checkCudaError(cudaError_t err, const char* const file, int const line, char const* const func, const char* f);
    }
}