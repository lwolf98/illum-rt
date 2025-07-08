#pragma once
#include "kernels.h"
#include "cuda-operators.h"
#include "trace-helper.cuh"
#include "base.h"

#define EPSILON 0.000001 // Moeller-Trumbore triangle intersection

#define INTERSECT_BOX_PARAMETERS float3 &boxmin, float3 &boxmax, float3 &ray_o, float3 &ray_d, float3 &ray_id, float3 &ray_ood, const float &t_min, const float &t_max, float &hit_t
__forceinline__ __device__ bool intersect_box(INTERSECT_BOX_PARAMETERS);
__forceinline__ __device__ bool intersect_box_shirley(INTERSECT_BOX_PARAMETERS);
__forceinline__ __device__ bool intersect_box_aila(INTERSECT_BOX_PARAMETERS);

#define INTERSECT_TRIANGLE_PARAMETERS const float3 &v1, const float3 &v2, const float3 &v3, const float3 &ray_o, const float3 &ray_d, const float t_min, const float t_max, float &hit_t, float &hit_beta, float &hit_gamma, bool debug
__forceinline__ __device__ bool intersect_triangle(INTERSECT_TRIANGLE_PARAMETERS = false);
__forceinline__ __device__ bool intersect_triangle_shirley(INTERSECT_TRIANGLE_PARAMETERS = false);
__forceinline__ __device__ bool intersect_triangle_moeller_trumbore(INTERSECT_TRIANGLE_PARAMETERS = false);
__forceinline__ __device__ bool is_below_alpha_threshold(const wf::cuda::tri_is &intersection, 
                                                         const uint4 &tri,
                                                         const wf::cuda::material *materials,
                                                         const float2 *tex_coords);

__forceinline__ __device__ int vmin_max (int a, int b, int c);
__forceinline__ __device__ int vmax_min (int a, int b, int c);
__forceinline__ __device__ int vmin_min (int a, int b, int c);
__forceinline__ __device__ int vmax_max (int a, int b, int c);

__forceinline__ __device__ float3 f4_to_f3(float4 v) {
	return make_float3(v.x, v.y, v.z);
}

__forceinline__ __device__ float3 ray_id(float3 ray_d) {
	float3 ray_id;
	const float ooeps = exp2f(-80.f); // avoid div by zero
	ray_id.x = 1.0f/ (fabsf(ray_d.x) > ooeps ? ray_d.x : copysignf(ooeps, ray_d.x));
	ray_id.y = 1.0f/ (fabsf(ray_d.y) > ooeps ? ray_d.y : copysignf(ooeps, ray_d.y));
	ray_id.z = 1.0f/ (fabsf(ray_d.z) > ooeps ? ray_d.z : copysignf(ooeps, ray_d.z));
	return ray_id;
}

__forceinline__ __device__ float3 ray_ood(float3 ray_o, float3 ray_id) {
	return make_float3(ray_o.x*ray_id.x, ray_o.y*ray_id.y, ray_o.z*ray_id.z);
}

// Intersection tests
__forceinline__ __device__ bool intersect_box(INTERSECT_BOX_PARAMETERS) {
    // return intersect_box_shirley(boxmin, boxmax, ray_o, ray_d, ray_id, ray_ood, t_min, t_max, hit_t);
    return intersect_box_aila(boxmin, boxmax, ray_o, ray_d, ray_id, ray_ood, t_min, t_max, hit_t);
}
__forceinline__ __device__ bool intersect_box_shirley(INTERSECT_BOX_PARAMETERS) {
    const float t1x_tmp = (boxmin.x - ray_o.x) * ray_id.x;
    const float t2x_tmp = (boxmax.x - ray_o.x) * ray_id.x;
    const float t1x = (t1x_tmp < t2x_tmp) ? t1x_tmp : t2x_tmp;
    const float t2x = (t2x_tmp < t1x_tmp) ? t1x_tmp : t2x_tmp;

    const float t1y_tmp = (boxmin.y - ray_o.y) * ray_id.y;
    const float t2y_tmp = (boxmax.y - ray_o.y) * ray_id.y;
    const float t1y = (t1y_tmp < t2y_tmp) ? t1y_tmp : t2y_tmp;
    const float t2y = (t2y_tmp < t1y_tmp) ? t1y_tmp : t2y_tmp;

    const float t1z_tmp = (boxmin.z - ray_o.z) * ray_id.z;
    const float t2z_tmp = (boxmax.z - ray_o.z) * ray_id.z;
    const float t1z = (t1z_tmp < t2z_tmp) ? t1z_tmp : t2z_tmp;
    const float t2z = (t2z_tmp < t1z_tmp) ? t1z_tmp : t2z_tmp;

    float t1 = (t1x < t1y) ? t1y : t1x;
          t1 = (t1z < t1) ? t1  : t1z;
    float t2 = (t2x < t2y) ? t2x : t2y;
          t2 = (t2z < t2) ? t2z : t2;

    if (t1 > t2)    return false;
    if (t2 < t_min) return false;
    if (t1 > t_max) return false;

    hit_t = t1;
    return true;
}
__forceinline__ __device__ bool intersect_box_aila(INTERSECT_BOX_PARAMETERS) {
    // Following Aila, Laine, Karras: Understanding the efficiency of ray traversal on GPUs–Kepler and Fermi addendum

    float x0 = boxmin.x * ray_id.x - ray_ood.x;
    float y0 = boxmin.y * ray_id.y - ray_ood.y;
    float z0 = boxmin.z * ray_id.z - ray_ood.z;

    float x1 = boxmax.x * ray_id.x - ray_ood.x;
    float y1 = boxmax.y * ray_id.y - ray_ood.y;
    float z1 = boxmax.z * ray_id.z - ray_ood.z;

    // Using VMIN/VMAX assembly instructions
    // A) -Kepler code variant (Aila/Laine) (this requires t_min to be nonnegative to be correct) - 72ms
    // float tminbox = __int_as_float(
    //                     vmax_max(
    //                     __float_as_int(fminf(x0,x1)),
    //                     __float_as_int(fminf(y0,y1)),
    //                     vmin_max(
    //                         __float_as_int(z0),
    //                         __float_as_int(z1),
    //                         __float_as_int(t_min))));
    // float tmaxbox = __int_as_float(
    //                     vmin_min(
    //                         __float_as_int(fmaxf(x0,x1)),
    //                         __float_as_int(fmaxf(y0,y1)),
    //                         vmax_min(
    //                             __float_as_int(z0),
    //                             __float_as_int(z1),
    //                             __float_as_int(t_max))));

    // B) -Fermi code variant (Aila/Laine) (this requires t_min to be nonnegative to be correct) - 87ms
    // float tminbox = __int_as_float(
    //                     vmin_max(
    //                         __float_as_int(z0),
    //                         __float_as_int(z1),
    //                         vmin_max(
    //                             __float_as_int(y0),
    //                             __float_as_int(y1),
    //                             vmin_max(
    //                                 __float_as_int(x0),
    //                                 __float_as_int(x1),
    //                                 __float_as_int(t_min)))));
    // float tmaxbox = __int_as_float(
    //                     vmax_min(
    //                         __float_as_int(z0),
    //                         __float_as_int(z1),
    //                         vmax_min(
    //                             __float_as_int(y0),
    //                             __float_as_int(y1),
    //                             vmax_min(
    //                                 __float_as_int(x0),
    //                                 __float_as_int(x1),
    //                                 __float_as_int(t_max)))));

    // C) -Paper variant VMIN/VMAX (this requires t_min to be nonnegative to be correct) - 87ms
    // float tminbox = __int_as_float(
    //                     vmin_max(
    //                         __float_as_int(x0),
    //                         __float_as_int(x1),
    //                         vmin_max(
    //                             __float_as_int(y0),
    //                             __float_as_int(y1),
    //                             vmin_max(
    //                                 __float_as_int(z0),
    //                                 __float_as_int(z1),
    //                                 __float_as_int(t_min)))));
    // float tmaxbox = __int_as_float(
    //                     vmax_min(
    //                     __float_as_int(x0),
    //                     __float_as_int(x1),
    //                     vmax_min(
    //                         __float_as_int(y0),
    //                         __float_as_int(y1),
    //                         vmax_min(
    //                             __float_as_int(z0),
    //                             __float_as_int(z1),
    //                             __float_as_int(t_max)))));

    // D) Using cuda integer instructions (this requires t_min to be nonnegative to be correct) - 38ms
    // float tminbox = __int_as_float(max(
    //                                     max(
    //                                         __float_as_int(t_min),
    //                                         min(__float_as_int(x0),
    //                                             __float_as_int(x1))),
    //                                     max(
    //                                         min(__float_as_int(y0),
    //                                             __float_as_int(y1)),
    //                                         min(__float_as_int(z0),
    //                                             __float_as_int(z1)))));
    // float tmaxbox = __int_as_float(min(
    //                                     min(
    //                                         __float_as_int(t_max),
    //                                         max(
    //                                             __float_as_int(x0),
    //                                             __float_as_int(x1))),
    //                                     min(
    //                                         max(
    //                                             __float_as_int(y0),
    //                                             __float_as_int(y1)),
    //                                         max(
    //                                             __float_as_int(z0),
    //                                             __float_as_int(z1)))));

    // E) -Tesla code variant (Aila/Laine), but only standard instructions - 38ms
    // float tminbox = fmaxf(
    //                     fmaxf(
    //                         fmaxf(
    //                             fminf(x0,x1),
    //                             fminf(y0,y1)),
    //                         fminf(z0,z1)),
    //                     t_min);
    // float tmaxbox = fminf(
    //                     fminf(
    //                         fminf(
    //                             fmaxf(x0,x1),
    //                             fmaxf(y0,y1)),
    //                         fmaxf(z0,z1)),
    //                     t_max);

    // (F) Using standard cuda float math instructions - 38ms
    float tminbox = fmaxf(fmaxf(t_min,
								fminf(x0,x1)),
						  fmaxf(fminf(y0,y1),
								fminf(z0,z1)));
    float tmaxbox = fminf(fminf(t_max,
								fmaxf(x0,x1)),
						  fminf(fmaxf(y0,y1),
								fmaxf(z0,z1)));

    bool intersect = (tmaxbox >= tminbox);
    hit_t = tminbox;
    return intersect;
}

__forceinline__ __device__ bool intersect_triangle(INTERSECT_TRIANGLE_PARAMETERS) {
    return intersect_triangle_shirley(v1, v2, v3, ray_o, ray_d, t_min, t_max, hit_t, hit_beta, hit_gamma, debug);
    // return intersect_triangle_moeller_trumbore(v1, v2, v3, ray_o, ray_d, t_min, t_max, hit_t, hit_beta, hit_gamma, debug);
}

__forceinline__ __device__ bool intersect_triangle_shirley(INTERSECT_TRIANGLE_PARAMETERS) {
    // Following Shirley: Fundamentals of Computer Graphics 4th ed., pp. 77-79
    const float a_x = v1.x;
    const float a_y = v1.y;
    const float a_z = v1.z;

    const float a = a_x - v2.x;
    const float b = a_y - v2.y;
    const float c = a_z - v2.z;

    const float d = a_x - v3.x;
    const float e = a_y - v3.y;
    const float f = a_z - v3.z;

    const float g = ray_d.x;
    const float h = ray_d.y;
    const float i = ray_d.z;

    const float j = a_x - ray_o.x;
    const float k = a_y - ray_o.y;
    const float l = a_z - ray_o.z;

    float common1 = e*i - h*f;
    float common2 = g*f - d*i;
    float common3 = d*h - e*g;
    float M 	  = a * common1  +  b * common2  +  c * common3;
    float beta 	  = j * common1  +  k * common2  +  l * common3;

    common1       = a*k - j*b;
    common2       = j*c - a*l;
    common3       = b*l - k*c;
    float gamma   = i * common1  +  h * common2  +  g * common3;
    float tt    = -(f * common1  +  e * common2  +  d * common3);

    beta /= M;
    gamma /= M;
    tt /= M;

    if (tt > t_min && tt < t_max)
        if (beta > 0 && gamma > 0 && beta + gamma <= 1) {
            hit_t = tt;
            hit_beta = beta;
            hit_gamma = gamma;
            return true;
        }
    return false;
}

__forceinline__ __device__ bool intersect_triangle_moeller_trumbore(INTERSECT_TRIANGLE_PARAMETERS) {
    // following Möller, Trumbore: Fast, Minimum Storage Ray/Triangle Intersection
    float3 edge1, edge2, tvec, pvec, qvec;
    float det, inv_det;

    edge1.x = v2.x - v1.x;
    edge1.y = v2.y - v1.y;
    edge1.z = v2.z - v1.z;

    edge2.x = v3.x - v1.x;
    edge2.y = v3.y - v1.y;
    edge2.z = v3.z - v1.z;

    cross(pvec, ray_d, edge2);
    det = dot(edge1, pvec);

	if (debug) printf("Start tri intersection\n");

    if (det > -EPSILON && det < EPSILON)
        return false;

	if (debug) printf("Passed epsilon check\n");

    tvec.x = ray_o.x - v1.x;
    tvec.y = ray_o.y - v1.y;
    tvec.z = ray_o.z - v1.z;

    hit_beta = dot(tvec, pvec);

    if (hit_beta < 0.0 || hit_beta > det)
        return false;

	if (debug) printf("Passed beta check\n");

    cross(qvec, tvec, edge1);
    hit_gamma = dot(ray_d, qvec);
    if (hit_gamma < 0.0 || hit_beta + hit_gamma > det)
        return false;

	if (debug) printf("Passed gamma/beta check\n");

	inv_det = 1.0/det;
    float tt = dot(edge2, qvec) * inv_det;
	if (tt > t_min && tt < t_max) {
		if (debug) printf("Passed t check\n");
		hit_t = tt;
		hit_beta *= inv_det;
		hit_gamma *= inv_det;
		return true;
	}
	return false;
}

 __forceinline__ __device__ int vmin_max (int a, int b, int c) {
    int ret;
    asm("vmin.s32.s32.s32.max %0, %1, %2, %3;" : "=r"(ret) : "r"(a), "r"(b), "r"(c));
    return ret;
}
__forceinline__ __device__ int vmax_min (int a, int b, int c) {
    int val;
    asm("vmax.s32.s32.s32.min %0, %1, %2, %3;" : "=r"(val) : "r"(a), "r"(b), "r"(c));
    return val;
}
__forceinline__ __device__ int vmin_min (int a, int b, int c) {
    int val;
    asm("vmin.s32.s32.s32.min %0, %1, %2, %3;" : "=r"(val) : "r"(a), "r"(b), "r"(c));
    return val;
}
__forceinline__ __device__ int vmax_max (int a, int b, int c) {
    int val;
    asm("vmax.s32.s32.s32.max %0, %1, %2, %3;" : "=r"(val) : "r"(a), "r"(b), "r"(c));
    return val;
}

namespace wf {
    namespace cuda {
        static __device__ float3 f3(const float4 &v) { return make_float3(v.x, v.y, v.z); }

        struct __align__(16) diff_geom {
            __device__ __inline__ diff_geom(const wf::cuda::tri_is &is, const wf::cuda::scene_refs *params) {
                if (is.is_tri())
                    init_tri(is, params);
                else
                    init_custom_prim(is, params);
            }

            float2 tc;
            float3 x;
            float3 ng, ns;
            const wf::cuda::material *mat;

        private:
            __device__ __forceinline__ void init_base(const float3 vertex_pos_a, const float3 vertex_pos_b, const float3 vertex_pos_c,
                                                        const float3 vertex_norm_a, const float3 vertex_norm_b, const float3 vertex_norm_c,
                                                        const float2 vertex_tc_a, const float2 vertex_tc_b, const float2 vertex_tc_c,
                                                        const float2 barycentrics, const wf::cuda::material *mat) {

                float alpha = 1.f - barycentrics.x - barycentrics.y;
                x  = alpha * vertex_pos_a  + barycentrics.x * vertex_pos_b  + barycentrics.y * vertex_pos_c;
                tc = alpha * vertex_tc_a   + barycentrics.x * vertex_tc_b   + barycentrics.y * vertex_tc_c;
                ns = alpha * vertex_norm_a + barycentrics.x * vertex_norm_b + barycentrics.y * vertex_norm_c;
                ng = cross(vertex_pos_b-vertex_pos_a, vertex_pos_c-vertex_pos_a);
                normalize(ng);
                this->mat = mat;
            }

            __device__ __forceinline__ void init_tri(const wf::cuda::tri_is &is, const wf::cuda::scene_refs *params) {
                const unsigned int primitive_index = is.ref();
                const uint4 triangle = params->triangles[primitive_index];
                const float2 barycentrics = {.x = is.beta, .y = is.gamma};
                init_base(
                    f3(params->vertex_pos[triangle.x]), f3(params->vertex_pos[triangle.y]), f3(params->vertex_pos[triangle.z]),
                    f3(params->vertex_norm[triangle.x]), f3(params->vertex_norm[triangle.y]), f3(params->vertex_norm[triangle.z]),
                    params->vertex_tc[triangle.x], params->vertex_tc[triangle.y], params->vertex_tc[triangle.z],
                    barycentrics, &params->materials[triangle.w]
                );
            }

            __device__ __forceinline__ void init_custom_prim(const wf::cuda::tri_is &is, const wf::cuda::scene_refs *params) {
                uint32_t patch_ref = is.ref(); //((uint32_t)-1) - is.ref();
                bool upper = is.is_upper_tri() > 0;
                int32_t subd_quad_ref = is.quad_ref();

                const wf::cuda::subd_patch patch = params->patches[patch_ref];
                const uint4 tri = patch.subd_tri(subd_quad_ref, upper);

                const float2 barycentrics = {.x = is.beta, .y = is.gamma};
                init_base(
                    f3(params->patch_vertex_pos[tri.x]), f3(params->patch_vertex_pos[tri.y]), f3(params->patch_vertex_pos[tri.z]),
                    f3(params->patch_vertex_norm[tri.x]), f3(params->patch_vertex_norm[tri.y]), f3(params->patch_vertex_norm[tri.z]),
                    params->patch_vertex_tc[tri.x], params->patch_vertex_tc[tri.y], params->patch_vertex_tc[tri.z],
                    barycentrics, &params->materials[tri.w]
                );
            }
        };   
    }
}
