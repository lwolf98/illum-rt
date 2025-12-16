#pragma once
#include <vector_types.h>
#include <optix.h>
#include "driver/defines.h"

namespace wf::cuda {
    class ray;  
    class tri_is; 
    class material;
	class subd_patch;
	class subd_subpatch;
	class patch_node;

    struct optix_launch_params {
        int2 frame_buffer_dimensions;
        OptixTraversableHandle optix_traversable_handle;
        unsigned int ray_flags;
        float4 *rays;
        tri_is *triangle_intersections;
        material *materials;
        float2 *tex_coords;
        uint4 *triangles;

		subd_patch *patches;
		subd_subpatch *subpatches;
		patch_node *patch_nodes;
#ifndef BOX_APPROXIMATION
		float4 *patch_vertex_pos;
		float2 *patch_vertex_tc;
#endif

    };
}