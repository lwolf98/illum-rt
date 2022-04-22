#pragma once

#include <iostream>
#include <cuda.h>
#include <cuda_runtime.h>

#define CHECK_CUDA_ERROR(f,str) {cudaError_t err = (f); if (err) {wf::cuda::check_cuda_error(err, __FILE__, __LINE__, __PRETTY_FUNCTION__, std::string(#f)+": "+str); }}

static constexpr bool force_cuda_sync = true;

#define warn_on_cuda_error(X) CHECK_CUDA_ERROR(cudaGetLastError(), X)
#define potentially_sync_cuda(X) if (force_cuda_sync) CHECK_CUDA_ERROR(cudaDeviceSynchronize(), X)

namespace wf {
    namespace cuda {
        void check_cuda_error(cudaError_t err, const char* const file, int const line, char const* const func, const std::string &f);
    }
}
