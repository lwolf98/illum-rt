#include "cuda-helpers.h"

namespace wf {
    namespace cuda {
        void check_cuda_error(cudaError_t err, const std::string &file, const int line, const std::string &func, const std::string &f) {
            std::cerr << "Cuda Error: in file " << file << ":" << line << std::endl;
            std::cerr << "Cuda Error: in function " << func << std::endl;
            std::cerr << "Cuda Error: from " << f << std::endl;
            std::cerr << "Cuda Error: " << cudaGetErrorString(err) << " (" << cudaGetErrorName(err) << ")" << std::endl;
            cudaDeviceReset();
            throw std::runtime_error("CUDA Error");
        }
    }
}
