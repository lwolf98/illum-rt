#pragma once
#include <vector_types.h>
#include <optix.h>


namespace wf::cuda {
    class ray;  
    class tri_is; 

    struct optix_launch_params {
        int2 frame_buffer_dimensions;
        OptixTraversableHandle optix_traversable_handle;
        unsigned int ray_flags;
        float4 *rays;
        tri_is *triangle_intersections;
    };
}