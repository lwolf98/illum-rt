#include <optix_device.h>
#include "optix-launch-params.h"
#include <texture_indirect_functions.h>
#include <vector_types.h>
#include <vector_functions.h>
#include "cuda-operators.h"
#include "base.h"


namespace wf::cuda {

    extern "C" __constant__ optix_launch_params launch_params;
    
    static __forceinline__ __device__ void* unpack_pointer(uint32_t i0, uint32_t i1) {
        const uint64_t uptr = static_cast<uint64_t>(i0) << 32 | i1;
        void *ptr = reinterpret_cast<void*>(uptr);
        return ptr;
    };
    
    static __forceinline__ __device__ void pack_pointer(void *ptr, uint32_t &i0, uint32_t &i1) {
        const uint64_t uptr = reinterpret_cast<uint64_t>(ptr);
        i0 = uptr >> 32;
        i1 = uptr & 0x00000000FFFFFFFF;
    };

    template <typename T>
    static __forceinline__ __device__ T* per_ray_data() {
        const uint32_t u0 = optixGetPayload_0();
        const uint32_t u1 = optixGetPayload_1();
        return reinterpret_cast<T*>(unpack_pointer(u0, u1));
    };

    enum {SURFACE_RAY_TYPE = 0, RAY_TYPE_COUNT};

    extern "C" __global__ void __closesthit__radiance() {
        printf("closest hit");

        tri_is *prd = per_ray_data<tri_is>();

        prd->ref = optixGetPrimitiveIndex();
        const float2 barycentrics = optixGetTriangleBarycentrics();
        
        prd->beta = barycentrics.x;
        prd->gamma = barycentrics.y;
        prd->t = optixGetRayTmax();
    };
    
    constexpr const float ALPHA_THRESHOLD = 0.5f;
    extern "C" __global__ void __anyhit__radiance() {};
    extern "C" __global__ void __anyhit__radiance_alpha() {
        tri_is *prd = per_ray_data<tri_is>();
        const unsigned int primitive_index = optixGetPrimitiveIndex();
        const uint4 tri = launch_params.triangles[primitive_index];
        const float2 barycentrics = optixGetTriangleBarycentrics();
        const material m = launch_params.materials[tri.w];
        if (m.albedo_tex > 0) {
            float2 tc = (1.0f - barycentrics.x - barycentrics.y) * launch_params.tex_coords[tri.x]
                        + barycentrics.x * launch_params.tex_coords[tri.y] 
                        + barycentrics.y * launch_params.tex_coords[tri.z];
            float4 tex = tex2D<float4>(m.albedo_tex, tc.x, tc.y);
            if (tex.w < ALPHA_THRESHOLD)
                optixIgnoreIntersection();
        }
    };

	extern "C" __global__ void __closesthit__patches() {
		//TODO: implement
		//printf("closest hit patches");

		tri_is *prd = per_ray_data<tri_is>();

        prd->ref = optixGetPrimitiveIndex();
        //const float2 barycentrics = optixGetTriangleBarycentrics();
        //
        //prd->beta = barycentrics.x;
        //prd->gamma = barycentrics.y;
        prd->beta = 0.5f;
        prd->gamma = 0.5f;
        prd->t = optixGetRayTmax();
	};

	extern "C" __global__ void __anyhit__patches() {
		//TODO: implement
		printf("any hit patches");
	};

	extern "C" __global__ inline vec3 f3_to_vec3(float3 f3) {
		return vec3(f3.x, f3.y, f3.z);
	}

	extern "C" __global__ inline vec3 f4_to_vec3(float4 f4) {
		return f3_to_vec3(float3 {.x = f4.x, .y = f4.y, .z = f4.z});
	}

	extern "C" __global__ void __intersection__patches() {
		//TODO: implement
		//printf("intersection patches");

		int id = optixGetPrimitiveIndex();
		auto &patch = launch_params.patches[id];
		//printf("%d:%d\n", id, patch.subd_level);
		auto &node = launch_params.patch_nodes[patch.bvh_node];
		printf("right:%d, left:%d\n", node.right, node.left);

		float3 ray_origin = optixGetObjectRayOrigin();
		float3 ray_direction = optixGetObjectRayDirection();
		float tmin = optixGetRayTmin();
		float tmax = optixGetRayTmax();
		::ray is_ray(f3_to_vec3(ray_origin), f3_to_vec3(ray_direction));
		is_ray.t_min = tmin;
		is_ray.t_max = tmax;

		auto &node_left = launch_params.patch_nodes[node.left];
		auto &node_right = launch_params.patch_nodes[node.right];
		aabb box_left {}, box_right {};
		box_left.min = f4_to_vec3(node_left.min);
		box_left.max = f4_to_vec3(node_left.max);
		box_right.min = f4_to_vec3(node_right.min);
		box_right.max = f4_to_vec3(node_right.max);

		float t_l = FLT_MAX;
		intersect4(box_left, is_ray, t_l);

		float t_r = FLT_MAX;
		intersect4(box_right, is_ray, t_r);
		
		//optixReportIntersection(0.5f, 0);
		optixReportIntersection(t_l<t_r ? t_l:t_r, 0);
	};
    
    extern "C" __global__ void __miss__radiance() {};
    
    /* \brief The raygen program does not generate any rays in this case.
     * Since our algorithm has a dedicated step for generating rays we store
     * a pointer to those in constant memory / the launch params and use them
     * within this function to call optixTrace.
     */
     extern "C" __global__ void __raygen__render_frame() {
        const int ix = optixGetLaunchIndex().x;
        const int iy = optixGetLaunchIndex().y;

        tri_is intersection;
        
        uint32_t u0, u1;
        pack_pointer(&intersection, u0, u1);
        
        int pixel_index = ix + iy * launch_params.frame_buffer_dimensions.x;
        
        float4 ray_o_f4 = launch_params.rays[pixel_index * 2];
        float4 ray_d_f4 = launch_params.rays[pixel_index * 2 + 1];
    
        float3 ray_origin_f3  = make_float3(ray_o_f4.x, ray_o_f4.y, ray_o_f4.z);
        float3 ray_direction_f3  = make_float3(ray_d_f4.x, ray_d_f4.y, ray_d_f4.z);

        optixTrace(launch_params.optix_traversable_handle,
                   ray_origin_f3,
                   ray_direction_f3,
                   ray_o_f4.w,
                   ray_d_f4.w,
                   0.0f,
                   OptixVisibilityMask(255),
                   launch_params.ray_flags,
                   SURFACE_RAY_TYPE,
                   RAY_TYPE_COUNT,
                   SURFACE_RAY_TYPE,
                   u0,
                   u1);

        launch_params.triangle_intersections[pixel_index] = intersection;
    }
}
