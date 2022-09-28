#pragma once
#include <optix.h>
#include <cuda_runtime.h>

namespace wf::cuda {
    
    struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) raygen_record {
        __align__(OPTIX_SBT_RECORD_ALIGNMENT) char header[OPTIX_SBT_RECORD_HEADER_SIZE];

    };

    struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) miss_record {
        __align__(OPTIX_SBT_RECORD_ALIGNMENT) char header[OPTIX_SBT_RECORD_HEADER_SIZE];

    };

    struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) hitgroup_record {
        __align__(OPTIX_SBT_RECORD_ALIGNMENT) char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    };
}