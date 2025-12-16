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

		const auto &subpatch = launch_params.subpatches[optixGetPrimitiveIndex()];
		prd->set_ref(subpatch.parent_id, true);
        prd->beta = __uint_as_float(optixGetAttribute_0()); //0.5f;
        prd->gamma = __uint_as_float(optixGetAttribute_1()); //0.5f;
        prd->t = optixGetRayTmax();
		prd->subd_quad_ref = subd::quad_ref(optixGetAttribute_2());
		if (debug) printf("CH: t: %f, beta: %f, gamma: %f\n", prd->t, prd->beta, prd->gamma);
	};

	extern "C" __global__ void __anyhit__patches() {
		//TODO: implement
		if (DBG_PRINT) printf("any hit patches\n");
	};

	//TODO: find better place for these functions, maybe in base.h if possible with __clz(...)
	static uint32_t __forceinline__ __device__ log4_clz(uint32_t x) {
		return (31 - __clz(x)) >> 1;
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
			uint32_t trav_level = log4_clz(1+3*index);
			return child_node_base(trav_level, index);
	}
	//TODO end: until here

	// [FEAT-APPROX] Update logic for box approximation
	extern "C" __global__ void __intersection__patches() {
		if (DBG_PRINT) printf("intersection patches\n");
		uint3 px_index = optixGetLaunchIndex();
		bool debug = false;

		// node 6, patch 0 (level reg), quad 3 (lower)
		//debug = debug && px_index.x == 640 && px_index.y == 250;

		// node x, patch x (level -1), quad x (x)
		debug = debug && px_index.x == 528 && px_index.y == 251;

		int id = optixGetPrimitiveIndex();
		auto &subpatch = launch_params.subpatches[id];
		auto &patch = launch_params.patches[subpatch.parent_id];
		uint32_t node_offset = subpatch.bvh_node_offset;

		float3 ray_origin_world = optixGetObjectRayOrigin();
		float3 ray_direction_world = optixGetObjectRayDirection();
		float3 ray_origin = subpatch.trafo * ray_origin_world;
		float3 ray_direction = subpatch.trafo * ray_direction_world;
		float tmin = optixGetRayTmin();
		float tmax = optixGetRayTmax();
		float3 r_id = ray_id(ray_direction);
		float3 r_ood = ray_ood(ray_origin, r_id);

		// TMP: DEBUGGING
		//optixReportIntersection(0.f, 0, __float_as_uint(0.f), __float_as_uint(0.f), 1);
		//return;

		uint32_t stack[25];
		int32_t sp = 0;
		bool is_root_and_leaf = subpatch.subd_level == 0;
		stack[sp] = 0; // If subd_level is 0, the stack/this value is not used

		if (debug) printf("Ray origin: (%f %f %f)\n", ray_origin.x, ray_origin.y, ray_origin.z);
		if (debug) printf("Ray direction: (%f %f %f)\n", ray_direction.x, ray_direction.y, ray_direction.z);
		if (debug) printf("t_min: %f, t_max: %f\n", tmin, tmax);

		float closest_t = FLT_MAX;
		float beta = 0, gamma = 0;
		subd::quad_ref closest_quad_ref;
		while (sp >= 0) {
			if (debug) printf("\n");
			uint32_t index = stack[sp--];
			uint32_t trav_level = log4_clz(1+3*index);

			bool is_leaf = trav_level == subpatch.subd_level;
			if (!is_leaf) {
				const auto &node = launch_params.patch_nodes[node_offset + index];
				uint32_t child_base = child_node_base(trav_level, index);
				uint32_t off_current_level = geometric_series4(trav_level);
				float t_hit = FLT_MAX; // REVIEW: initialization required? and if yes, correct?
				float t_bary;

				for (int i = 0; i < 4; ++i) {
					float3 node_min = node.get_min(i);
					float3 node_max = node.get_max(i);

#ifndef PROJECTION
					if (!compute_valid_hit(node_min, node_max,							// box
									ray_origin, ray_direction, r_id, r_ood, tmin, tmax,	// ray
									closest_t, true,									// additional params
									t_hit, t_bary)) {									// reference/out params
										if (debug) printf("didn't hit node...\n");
										continue;
									}
#else
					/*if (!compute_valid_hit(node_min, node_max,							// box
									ray_origin, ray_direction, r_id, r_ood, tmin, tmax,	// ray
									closest_t, true, t1, p1_oriented, eps, subpatch,	// additional params
									t_hit, t_bary)) {									// reference/out params
										if (debug) printf("didn't hit node...\n");
										continue;
									}*/
#endif
					
#ifndef BOX_APPROXIMATION
					if (debug) printf("hit node!!!!!!\n");
					stack[++sp] = child_base+i;
#else
					if (trav_level < subpatch.subd_level-1) {
						if (debug) printf("hit node!!!!!!\n");
						stack[++sp] = child_base+i;
					}
					else {
						if (debug) printf("hit leaf box!!!!!!\n");

	#ifndef PROJECTION
						if (t_hit <= 0) continue;
	#endif
						//bary_calc(box, transformed_ray, t_bary, closest);
						bool upper_tri;
						bary_calc(node_min, node_max,							// box
									ray_origin, ray_direction, r_id, r_ood, tmin, tmax,	// ray
									t_bary, upper_tri, beta, gamma);
						closest_quad_ref.set_upper_tri(upper_tri);
						//closest.ref = ((uint32_t)-1) - patch_ref;
						closest_t = t_hit;

						uint32_t relative_index = (child_base+i) - off_current_level;
						uint32_t quad_ref_morton =    patch.index_from_quad_ref(subpatch.vert_start)
													+ relative_index;

						closest_quad_ref.set_ref(quad_ref_morton);
					}
#endif
				}
			}
			else {
#ifndef BOX_APPROXIMATION
				uint32_t quad_ref = subpatch.vert_start;
				if (!is_root_and_leaf) { // is only leaf
					uint32_t off_current_level = geometric_series4(trav_level-1);
					quad_ref += patch.quad_ref_from_index(index - off_current_level);
					if (debug) printf("Index: %d, trav_level: %d, off_cur_level: %d, quad_ref: %d\n", index, trav_level, off_current_level, quad_ref);
					if (debug) printf("Rel_index: %d, rel_quad_ref: %d\n", index - off_current_level, patch.quad_ref_from_index(index - off_current_level));
				}

				if (debug) printf("Patch ref: %d, Subpatch ref: %d, Quad ref: %d\n", subpatch.parent_id, id, quad_ref);
				if (debug) printf("Patch start index: %d\n", patch.start_index);
				if (debug) printf("Subpatch vert start: %d\n", subpatch.vert_start);

				uint4 tri_0 = patch.subd_tri(quad_ref, true);
				uint4 tri_1 = patch.subd_tri(quad_ref, false);
				if (debug) printf("Tri 0: %d %d %d\n", tri_0.x, tri_0.y, tri_0.z);
				if (debug) printf("Tri 1: %d %d %d\n", tri_1.x, tri_1.y, tri_1.z);
				float3 a_0 = f3(launch_params.patch_vertex_pos[tri_0.x]);
				float3 b_0 = f3(launch_params.patch_vertex_pos[tri_0.y]);
				float3 c_0 = f3(launch_params.patch_vertex_pos[tri_0.z]);
				float3 a_1 = f3(launch_params.patch_vertex_pos[tri_1.x]);
				float3 b_1 = f3(launch_params.patch_vertex_pos[tri_1.y]);
				float3 c_1 = f3(launch_params.patch_vertex_pos[tri_1.z]);
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
												ray_origin_world, ray_direction_world, tmin, tmax,
												t, beta, gamma, debug);

				
				if (debug) {
					printf("beta: %f, gamma: %f\n", beta, gamma);
					if (hit) printf("hit tri 0\n");
					else printf("did not hit tri 0\n");
				}
				if (hit && t < closest_t) {
					if (debug) printf("hit tri 0 and is closer!!!\n");
					closest_t = t;
					closest_quad_ref.set_ref(quad_ref);
					closest_quad_ref.set_upper_tri(true);
					continue;
				}

				t = FLT_MAX;
				hit = intersect_triangle(a_1, b_1, c_1,
												ray_origin_world, ray_direction_world, tmin, tmax,
												t, beta, gamma, debug);

				if (debug) {
					printf("beta: %f, gamma: %f\n", beta, gamma);
					if (hit) printf("hit tri 1\n");
					else printf("did not hit tri 1\n");
				}
				if (hit && t < closest_t) {
					if (debug) printf("hit tri 1 and is closer!!!\n");
					closest_t = t;
					closest_quad_ref.set_ref(quad_ref);
					closest_quad_ref.set_upper_tri(false);
				}
#else
					float t_hit = FLT_MAX; // REVIEW: initialization required? and if yes, correct?
					float t_bary;
					float3 root_min = f3(subpatch.root_min); // REVIEW: more efficient way without copying?
					float3 root_max = f3(subpatch.root_max);
	#ifndef PROJECTION
					if (!compute_valid_hit(root_min, root_max,							// box
									ray_origin, ray_direction, r_id, r_ood, tmin, tmax,	// ray
									closest_t, false,									// additional params
									t_hit, t_bary)) {									// reference/out params
										if (debug) printf("didn't hit node...\n");
										continue;
									}
	#else
					/*if (!compute_valid_hit(root_min, root_max,						// box
									ray_origin, ray_direction, r_id, r_ood, tmin, tmax,	// ray
									closest_t, false, t1, p1_oriented, eps, subpatch,	// additional params
									t_hit, t_bary)) {									// reference/out params
										if (debug) printf("didn't hit node...\n");
										continue;
									}*/
	#endif
					bool upper_tri;
					bary_calc(root_min, root_max,									// box
								ray_origin, ray_direction, r_id, r_ood, tmin, tmax,	// ray
								t_bary, upper_tri, beta, gamma);
					closest_quad_ref.set_upper_tri(upper_tri);
					closest_t = t_hit;

					uint32_t quad_ref_morton =    patch.index_from_quad_ref(subpatch.vert_start);
					closest_quad_ref.set_ref(quad_ref_morton);
#endif
			}
		}

		if (debug) printf("Closest t: %f\n", closest_t);

		if (closest_t < FLT_MAX)
			optixReportIntersection(closest_t, 0, __float_as_uint(beta), __float_as_uint(gamma), closest_quad_ref.internal_data());

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
