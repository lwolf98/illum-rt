#include <optix_device.h>
#include "optix-launch-params.h"
#include <texture_indirect_functions.h>
#include <vector_types.h>
#include <vector_functions.h>
#include "cuda-operators.h"
#include "base.h"
#include "trace-helper.cuh"


namespace wf::cuda {
    extern "C" __constant__ optix_launch_params launch_params;
	#define DBG_PRINT false
    
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
        if (DBG_PRINT) printf("closest hit\n");

        tri_is *prd = per_ray_data<tri_is>();

		prd->set_ref(optixGetPrimitiveIndex());
        const float2 barycentrics = optixGetTriangleBarycentrics();
        
        prd->beta = barycentrics.x;
        prd->gamma = barycentrics.y;
        prd->t = optixGetRayTmax();
    };
    
    constexpr const float ALPHA_THRESHOLD = 0.5f;
    extern "C" __global__ void __anyhit__radiance() {if (DBG_PRINT) printf("anyhit no alpha\n");};
    extern "C" __global__ void __anyhit__radiance_alpha() {
		if (DBG_PRINT) printf("anyhit alpha\n");
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
		if (DBG_PRINT) printf("closest hit patches\n");
		uint3 px_index = optixGetLaunchIndex();
		bool debug = false;

		// node 6, patch 0 (level reg), quad 3 (lower)
		//debug = debug && px_index.x == 640 && px_index.y == 250;

		// node x, patch x (level -1), quad x (x)
		debug = debug && px_index.x == 566 && px_index.y == 281;

		tri_is *prd = per_ray_data<tri_is>();

		optixGetLaunchIndex();

		prd->set_ref(optixGetPrimitiveIndex(), true);
        prd->beta = __uint_as_float(optixGetAttribute_0()); //0.5f;
        prd->gamma = __uint_as_float(optixGetAttribute_1()); //0.5f;
        prd->t = optixGetRayTmax();
		int32_t quad_ref = (int32_t)optixGetAttribute_2();
		prd->set_quad_ref(abs(quad_ref)-1, quad_ref >= 0);
		if (debug) printf("CH: t: %f, beta: %f, gamma: %f\n", prd->t, prd->beta, prd->gamma);
	};

	extern "C" __global__ void __anyhit__patches() {
		//TODO: implement
		if (DBG_PRINT) printf("any hit patches\n");
	};

	//TODO: find better place for these functions
	static __forceinline__ __device__ int geometric_series4(int iterations) {
		return (1 - (1 << ((iterations+1)<<1))) / (-3);
	}

	static uint32_t __forceinline__ __device__ child_node_base(
			uint32_t trav_level,
			uint32_t index
		) {
			uint32_t off_current_level = geometric_series4(trav_level-1);
			uint32_t off_child_level = geometric_series4(trav_level);
			uint32_t idx_current_relative = index - off_current_level;
			uint32_t idx_child_relative = idx_current_relative << 2; //(* 4)
			uint32_t index_child = off_child_level + idx_child_relative;
			return index_child;
	}

	static uint32_t __forceinline__ __device__ child_node_base(
			uint32_t index
		) {
			uint32_t trav_level = (uint32_t) (0.5f*log2f(1+3*index));
			return child_node_base(trav_level, index);
	}
	//TODO end: until here

	extern "C" __global__ void __intersection__patches() {
		if (DBG_PRINT) printf("intersection patches\n");
		uint3 px_index = optixGetLaunchIndex();
		bool debug = false;

		// node 6, patch 0 (level reg), quad 3 (lower)
		//debug = debug && px_index.x == 640 && px_index.y == 250;

		// node x, patch x (level -1), quad x (x)
		debug = debug && px_index.x == 566 && px_index.y == 281;

		float3 ray_origin = optixGetObjectRayOrigin();
		float3 ray_direction = optixGetObjectRayDirection();
		float tmin = optixGetRayTmin();
		float tmax = optixGetRayTmax();
		float3 r_id = ray_id(ray_direction);
		float3 r_ood = ray_ood(ray_origin, r_id);

		int id = optixGetPrimitiveIndex();
		auto &patch = launch_params.patches[id];
		uint32_t node_offset = patch.bvh_node_offset;

		uint32_t stack[25];
		int32_t sp = 0;
		bool is_root_and_leaf = patch.subd_level == 0;
		stack[sp] = 0; // If subd_level is 0, the stack/this value is not used

		if (debug) printf("Ray origin: (%f %f %f)\n", ray_origin.x, ray_origin.y, ray_origin.z);
		if (debug) printf("Ray direction: (%f %f %f)\n", ray_direction.x, ray_direction.y, ray_direction.z);
		if (debug) printf("t_min: %f, t_max: %f\n", tmin, tmax);

		float closest_t = FLT_MAX;
		float beta = 0, gamma = 0;
		int32_t closest_quad_ref = 0;
		while (sp >= 0) {
			if (debug) printf("\n");
			uint32_t index = stack[sp--];
			uint32_t trav_level = (uint32_t) (0.5f*log2f(1+3*index));

			bool is_leaf = trav_level == patch.subd_level;
			if (!is_leaf) {
				const auto &node = launch_params.patch_nodes[node_offset + index];
				float t = FLT_MAX;

				for (int i = 0; i < 4; ++i) {
					float3 node_min = node.get_min(i);
					float3 node_max = node.get_max(i);
					bool hit = intersect_box(node_min, node_max,
									ray_origin, ray_direction, r_id, r_ood,
									tmin, tmax, t);

					if (hit && t < closest_t) {
						if (debug) printf("hit node!!!!!!\n");
						uint32_t child_base = child_node_base(trav_level, index);
						stack[++sp] = child_base+i;
					}
					else if (debug) printf("didn't hit node...\n");
				}
			}
			else {
				uint32_t quad_ref = 0;
				if (!is_root_and_leaf) { // is only leaf
					uint32_t off_current_level = geometric_series4(trav_level-1);
					quad_ref = patch.quad_ref_from_index(index - off_current_level);
				}

				if (debug) printf("Patch ref: %d, Quad ref: %d\n", id, quad_ref);
				if (debug) printf("Patch start index: %d\n", patch.start_index);

				uint4 tri_0 = patch.subd_tri(quad_ref, true);
				uint4 tri_1 = patch.subd_tri(quad_ref, false);
				if (debug) printf("Tri 0: %d %d %d\n", tri_0.x, tri_0.y, tri_0.z);
				if (debug) printf("Tri 1: %d %d %d\n", tri_1.x, tri_1.y, tri_1.z);
				float3 a_0 = f4_to_f3(launch_params.patch_vertex_pos[tri_0.x]);
				float3 b_0 = f4_to_f3(launch_params.patch_vertex_pos[tri_0.y]);
				float3 c_0 = f4_to_f3(launch_params.patch_vertex_pos[tri_0.z]);
				float3 a_1 = f4_to_f3(launch_params.patch_vertex_pos[tri_1.x]);
				float3 b_1 = f4_to_f3(launch_params.patch_vertex_pos[tri_1.y]);
				float3 c_1 = f4_to_f3(launch_params.patch_vertex_pos[tri_1.z]);
				if (debug) {
					printf("t_min: %f, t_max: %f\n", tmin, tmax);
					printf("Tri 0 coords: (%f %f %f)", a_0.x, a_0.y, a_0.z);
					printf(" (%f %f %f)", b_0.x, b_0.y, b_0.z);
					printf(" (%f %f %f)\n", c_0.x, c_0.y, c_0.z);
					printf("Tri 1 coords: (%f %f %f)", a_1.x, a_1.y, a_1.z);
					printf(" (%f %f %f)", b_1.x, b_1.y, b_1.z);
					printf(" (%f %f %f)\n", c_1.x, c_1.y, c_1.z);
				}

				float t = FLT_MAX;
				bool hit = intersect_triangle(a_0, b_0, c_0,
												ray_origin, ray_direction, tmin, tmax,
												t, beta, gamma, debug);

				
				if (debug) {
					printf("beta: %f, gamma: %f\n", beta, gamma);
					if (hit) printf("hit tri 0\n");
					else printf("did not hit tri 0\n");
				}
				if (hit && t < closest_t) {
					if (debug) printf("hit tri 0 and is closer!!!\n");
					closest_t = t;
					closest_quad_ref = quad_ref+1;
					continue;
				}

				t = FLT_MAX;
				hit = intersect_triangle(a_1, b_1, c_1,
												ray_origin, ray_direction, tmin, tmax,
												t, beta, gamma, debug);

				if (debug) {
					printf("beta: %f, gamma: %f\n", beta, gamma);
					if (hit) printf("hit tri 1\n");
					else printf("did not hit tri 1\n");
				}
				if (hit && t < closest_t) {
					if (debug) printf("hit tri 1 and is closer!!!\n");
					closest_t = t;
					closest_quad_ref = (quad_ref+1) * -1;
				}
			}
		}

		//printf("hit:%d\n", closest_t < FLT_MAX);
		if (closest_t < FLT_MAX)
			optixReportIntersection(closest_t, 0, __float_as_uint(beta), __float_as_uint(gamma), closest_quad_ref);

	};
    
    extern "C" __global__ void __miss__radiance() {
		if (DBG_PRINT) printf("miss!!!!!!!!\n");
	};
    
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

		debug_info dbg;
		dbg.px_index.x = ix;
		dbg.px_index.y = iy;
		uint32_t u2, u3;
		pack_pointer(&dbg, u2, u3);
        
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
                   u0, u1);

        launch_params.triangle_intersections[pixel_index] = intersection;
    }
}
