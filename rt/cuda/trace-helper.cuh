#pragma once
#include "kernels.h"
#include "cuda-operators.h"
#include "base.h"

#define EPSILON 0.000001 // Moeller-Trumbore triangle intersection

#define BOX_RAY_PARAMETERS float3 &boxmin, float3 &boxmax, float3 &ray_o, float3 &ray_d, float3 &ray_id, float3 &ray_ood, const float &t_min, const float &t_max
//#define INTERSECT_BOX_PARAMETERS float3 &boxmin, float3 &boxmax, float3 &ray_o, float3 &ray_d, float3 &ray_id, float3 &ray_ood, const float &t_min, const float &t_max, float &hit_t
#define INTERSECT_BOX_PARAMETERS BOX_RAY_PARAMETERS, float &hit_t
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

__forceinline__ __device__ float3 f3(const float4 &v) {
	return make_float3(v.x, v.y, v.z);
}

__forceinline__ __device__ float3 ray_id(const float3 &ray_d) {
	float3 ray_id;
	const float ooeps = exp2f(-80.f); // avoid div by zero
	ray_id.x = 1.0f/ (fabsf(ray_d.x) > ooeps ? ray_d.x : copysignf(ooeps, ray_d.x));
	ray_id.y = 1.0f/ (fabsf(ray_d.y) > ooeps ? ray_d.y : copysignf(ooeps, ray_d.y));
	ray_id.z = 1.0f/ (fabsf(ray_d.z) > ooeps ? ray_d.z : copysignf(ooeps, ray_d.z));
	return ray_id;
}

//TODO: float3 as const reference
__forceinline__ __device__ float3 ray_ood(const float3 &ray_o, const float3 &ray_id) {
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
__forceinline__ __device__ bool intersect_box_shirley_extended(INTERSECT_BOX_PARAMETERS, uint32_t &hit_side) {
	hit_side = BOX_SIDE_UNDEFINED;
    const float t1x_tmp = (boxmin.x - ray_o.x) * ray_id.x;
    const float t2x_tmp = (boxmax.x - ray_o.x) * ray_id.x;
	uint32_t x_side = (t1x_tmp < t2x_tmp) ? BOX_SIDE_SIDE_DOWN : BOX_SIDE_SIDE_UP;
    const float t1x = (t1x_tmp < t2x_tmp) ? t1x_tmp : t2x_tmp;
    const float t2x = (t2x_tmp < t1x_tmp) ? t1x_tmp : t2x_tmp;

    const float t1y_tmp = (boxmin.y - ray_o.y) * ray_id.y;
    const float t2y_tmp = (boxmax.y - ray_o.y) * ray_id.y;
    const float t1y = (t1y_tmp < t2y_tmp) ? t1y_tmp : t2y_tmp;
    const float t2y = (t2y_tmp < t1y_tmp) ? t1y_tmp : t2y_tmp;
	uint32_t y_side = (t1y_tmp < t2y_tmp) ? BOX_SIDE_FRONT : BOX_SIDE_BACK;

    const float t1z_tmp = (boxmin.z - ray_o.z) * ray_id.z;
    const float t2z_tmp = (boxmax.z - ray_o.z) * ray_id.z;
    const float t1z = (t1z_tmp < t2z_tmp) ? t1z_tmp : t2z_tmp;
    const float t2z = (t2z_tmp < t1z_tmp) ? t1z_tmp : t2z_tmp;
	uint32_t z_side = (t1z_tmp < t2z_tmp) ? BOX_SIDE_SIDE_RIGHT : BOX_SIDE_SIDE_LEFT;

	uint32_t hit_side_min = (t1x < t1y) ? y_side : x_side;
    float              t1 = (t1x < t1y) ? t1y : t1x;
	         hit_side_min = (t1z < t1 ) ? hit_side_min : z_side;
                       t1 = (t1z < t1) ? t1  : t1z;
    float t2 = (t2x < t2y) ? t2x : t2y;
          t2 = (t2z < t2) ? t2z : t2;

    if (t1 > t2)    return false;
    if (t2 < t_min) return false;
    if (t1 > t_max) return false;

    hit_t = t1;
	hit_side = hit_side_min;
    return true;
}
__forceinline__ __device__ bool intersect_box_shirley_midbox(INTERSECT_BOX_PARAMETERS, uint32_t &hit_side) {
	hit_side = BOX_SIDE_UNDEFINED;
    const float t1x_tmp = (boxmin.x - ray_o.x) * ray_id.x;
    const float t2x_tmp = (boxmax.x - ray_o.x) * ray_id.x;
	uint32_t x_side_near = (t1x_tmp <= t2x_tmp) ? BOX_SIDE_SIDE_DOWN : BOX_SIDE_SIDE_UP;
	uint32_t x_side_far = (t1x_tmp > t2x_tmp) ? BOX_SIDE_SIDE_DOWN : BOX_SIDE_SIDE_UP;
    const float t1x = (t1x_tmp < t2x_tmp) ? t1x_tmp : t2x_tmp;
    const float t2x = (t2x_tmp < t1x_tmp) ? t1x_tmp : t2x_tmp;

    const float t1y_tmp = (boxmin.y - ray_o.y) * ray_id.y;
    const float t2y_tmp = (boxmax.y - ray_o.y) * ray_id.y;
    const float t1y = (t1y_tmp < t2y_tmp) ? t1y_tmp : t2y_tmp;
    const float t2y = (t2y_tmp < t1y_tmp) ? t1y_tmp : t2y_tmp;
	uint32_t y_side_near = (t1y_tmp <= t2y_tmp) ? BOX_SIDE_FRONT : BOX_SIDE_BACK;
	uint32_t y_side_far = (t1y_tmp > t2y_tmp) ? BOX_SIDE_FRONT : BOX_SIDE_BACK;

    const float t1z_tmp = (boxmin.z - ray_o.z) * ray_id.z;
    const float t2z_tmp = (boxmax.z - ray_o.z) * ray_id.z;
    const float t1z = (t1z_tmp < t2z_tmp) ? t1z_tmp : t2z_tmp;
    const float t2z = (t2z_tmp < t1z_tmp) ? t1z_tmp : t2z_tmp;
	uint32_t z_side_near = (t1z_tmp <= t2z_tmp) ? BOX_SIDE_SIDE_RIGHT : BOX_SIDE_SIDE_LEFT;
	uint32_t z_side_far = (t1z_tmp > t2z_tmp) ? BOX_SIDE_SIDE_RIGHT : BOX_SIDE_SIDE_LEFT;

	uint32_t hit_side_min = (t1x < t1y) ? y_side_near : x_side_near;
    float              t1 = (t1x < t1y) ? t1y : t1x;
	         hit_side_min = (t1z < t1 ) ? hit_side_min : z_side_near;
                       t1 = (t1z < t1) ? t1  : t1z;
	uint32_t hit_side_max = (t2x < t2y) ? x_side_far : y_side_far;
    float              t2 = (t2x < t2y) ? t2x : t2y;
	         hit_side_max = (t2z < t2 ) ? z_side_far : hit_side_max;
                       t2 = (t2z < t2) ? t2z : t2;

    if (t1 > t2)    return false;
    if (t2 < t_min) return false;

    float t_hit = (t1 < t_min) ? t2 : t1;
	if (t_hit < t_min) return false;
	if (t_hit > t_max) return false;

    hit_t = t_hit;
	hit_side = (t1 < t_min) ? hit_side_max : hit_side_min;
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

//__forceinline__ __device__ void bary_calc(const aabb &box, const ray &ray, float t_dist, triangle_intersection &is) {
__forceinline__ __device__ void bary_calc(BOX_RAY_PARAMETERS, float t_dist, bool &upper_tri, float &beta, float &gamma) {
	float3 hit = ray_o + t_dist * ray_d;
	float width = boxmax.x - boxmin.x;
	float height = boxmax.z - boxmin.z;
	float3 hit_relative = hit - boxmin;
	float2 hit_xy = make_float2(hit_relative.x, hit_relative.z);
	hit_xy.x = hit_xy.x / width;
	hit_xy.y = hit_xy.y / height;
	//assert(hit_xy.x >= -eps && hit_xy.x <= 1+eps);
	//assert(hit_xy.y >= -eps && hit_xy.y <= 1+eps);
	if (hit_xy.x < 0) hit_xy.x = 0;
	if (hit_xy.x > 1) hit_xy.x = 1;
	if (hit_xy.y < 0) hit_xy.y = 0;
	if (hit_xy.y > 1) hit_xy.y = 1;

	//bool upper_tri = hit_xy.y < -hit_xy.x + 1;
	//is.subd_quad_ref.set_upper_tri(upper_tri);
	upper_tri = hit_xy.y < -hit_xy.x + 1;
	if (upper_tri) {
		beta = hit_xy.x;
		gamma = hit_xy.y;
	}
	else {
		beta = 1 - hit_xy.x;
		gamma = 1 - hit_xy.y;
	}
	//assert(is.beta >= 0 && is.beta <= 1);
	//assert(is.gamma >= 0 && is.gamma <= 1);
	//assert(is.beta + is.gamma >= 0);
	//assert(is.beta + is.gamma <= 1);
}

__forceinline__ __device__ bool compute_valid_hit(
	//INTERSECT_BOX_PARAMETERS,
	BOX_RAY_PARAMETERS,
	float closest_t,
#ifdef PROJECTION
	float t_near_oriented,
	const float3 &hit_near_oriented,
	float t_off,
	const wf::cuda::subd_subpatch &subpatch,
#endif
	bool allow_negative_t,
	bool intersect_box_mid,
	float &hit_t,
	float &bary_t,
	uint32_t &hit_side
) {
	//if (!intersect4(box, transformed_ray, dist)) return false;
	//if (!intersect_box(INTERSECT_BOX_PARAMETERS)) return false;
	float dist;
	//if (!intersect_box(boxmin, boxmax, ray_o, ray_d, ray_id, ray_ood, t_min, t_max, dist)) return false;
	if (!intersect_box_shirley_extended(boxmin, boxmax, ray_o, ray_d, ray_id, ray_ood, t_min, t_max, dist, hit_side)) return false;
#ifdef BOX_MID_INTERSECTION
	#ifndef BOX_MID_VAR_FLAT
		intersect_box_mid = intersect_box_mid && (hit_side == BOX_SIDE_FRONT || hit_side == BOX_SIDE_BACK);
	#endif
	if (intersect_box_mid) {
		float mid = boxmin.y + (boxmax.y - boxmin.y)*0.5f;
		float3 mid_box_min = boxmin; //make_float3(boxmin.x, mid, boxmin.z);
		float3 mid_box_max = boxmax; //make_float3(boxmax.x, mid+eps, boxmax.z);
	#ifdef BOX_MID_VAR_FLAT
		constexpr float eps = 1e-6; //TODO: switch this IS with quad intersection, then no eps required
		//float3 mid_box_min = make_float3(boxmin.x, mid, boxmin.z);
		//float3 mid_box_max = make_float3(boxmax.x, mid+eps, boxmax.z);
		mid_box_min.y = mid;
		mid_box_max.y = mid+eps;
		if (!intersect_box_shirley_extended(mid_box_min, mid_box_max, ray_o, ray_d, ray_id, ray_ood, t_min, t_max, dist, hit_side)) return false;
	#else
		#ifdef BOX_MID_VAR_CARDBOX
			constexpr float eps = 1e-4f; // ray t epsilon to prevent self intersection -> see rt.h
			//mid_box_max.y = mid;
			float t_min_mid = dist + eps;
			if (!intersect_box_shirley_midbox(mid_box_min, mid_box_max, ray_o, ray_d, ray_id, ray_ood, t_min_mid, t_max, dist, hit_side)) {
				#ifdef BOX_MID_SUPPORT_BACK_SIDE
					mid_box_max.y = boxmax.y;
					mid_box_min.y = mid;
					if (!intersect_box_shirley_midbox(mid_box_min, mid_box_max, ray_o, ray_d, ray_id, ray_ood, t_min_mid, t_max, dist, hit_side)) return false;
				#else
					return false;
				#endif
			};
		#else
			// Projection Variant
		#endif
	#endif
	}
#endif

#ifndef PROJECTION
	//assert(!std::isnan(dist)); // REVIEW: can this happen?
	if (isnan(dist)) return false;
	if (dist >= closest_t) return false;
	if (!allow_negative_t && dist <= 0) return false;
	hit_t = dist;
	bary_t = dist;
#else
	dist += t_off; // correct by earlier added epsilon

	// Calculate new t
	float t_total;
	float3 x_proj = ray_o + dist * ray_d;
	float3 x_oriented = subpatch.projected_to_oriented(x_proj);
	float t_dist = length(x_oriented - hit_near_oriented);
	t_total = t_near_oriented + t_dist;
	
	if (isnan(t_total)) return false; //REVIEW: check nans here
	if (t_total >= closest_t) return false; // -> fixes overlapping and shadow ray self intersection
	if (!allow_negative_t && t_total <= 0) return false;
	hit_t = t_total;
	bary_t = dist;
#endif

	return true;
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
		struct __align__(16) diff_geom {
			__device__ __inline__ diff_geom(const tri_is &is, const float3 &ray_org, const float3 &ray_dir, const scene_refs *params, int32_t dbg_x = -1, int32_t dbg_y = -1) {
				this->dbg_x = dbg_x;
				this->dbg_y = dbg_y;
				if (is.is_tri())
					init_tri(is, params);
				else
					init_custom_prim(is, ray_org, ray_dir, params);
			}

			float2 tc;
			float3 x;
			float3 ng, ns;
			const material *mat;

			// TODO/TMP: only debugging
			int32_t dbg_x, dbg_y;
			__device__ __forceinline__ bool debug() { return true && dbg_x == 566 && dbg_y == 281; }

			// This function provides a light weight diff_geom that does not require a ray, but also does not provie the hit point x.
			// The other fields are initialized correctly.
			// This might be used e.g. for primary hit.
			static __device__ __forceinline__ diff_geom init_lightweight(const tri_is &is, const scene_refs *params) {
				float3 dummy_org = {0,0,0};
				float3 dummy_dir = {0,0,0};
				return diff_geom(is, dummy_org, dummy_dir, params);
			}

		private:
			__device__ __forceinline__ void init_base(const float3 vertex_pos_a, const float3 vertex_pos_b, const float3 vertex_pos_c,
														const float3 vertex_norm_a, const float3 vertex_norm_b, const float3 vertex_norm_c,
														const float2 vertex_tc_a, const float2 vertex_tc_b, const float2 vertex_tc_c,
														const float2 barycentrics, const material *mat) {

				float alpha = 1.f - barycentrics.x - barycentrics.y;
				x  = alpha * vertex_pos_a  + barycentrics.x * vertex_pos_b  + barycentrics.y * vertex_pos_c;
				tc = alpha * vertex_tc_a   + barycentrics.x * vertex_tc_b   + barycentrics.y * vertex_tc_c;
				ns = alpha * vertex_norm_a + barycentrics.x * vertex_norm_b + barycentrics.y * vertex_norm_c;
				ng = cross(vertex_pos_b-vertex_pos_a, vertex_pos_c-vertex_pos_a);
				normalize(ng);
				this->mat = mat;
			}

			__device__ __forceinline__ void init_tri(const tri_is &is, const scene_refs *params) {
				const unsigned int primitive_index = is.ref();
				const uint4 triangle = params->triangles[primitive_index];
				const float2 barycentrics = {.x = is.beta, .y = is.gamma};
				init_base(
					f3(params->vertex_pos[triangle.x]), f3(params->vertex_pos[triangle.y]), f3(params->vertex_pos[triangle.z]),
					f3(params->vertex_norm[triangle.x]), f3(params->vertex_norm[triangle.y]), f3(params->vertex_norm[triangle.z]),
					params->vertex_tc[triangle.x], params->vertex_tc[triangle.y], params->vertex_tc[triangle.z],
					barycentrics, &params->materials[triangle.w]
				);
				//printf("Material: %d\n", triangle.w);
			}

			__device__ __forceinline__ void init_custom_prim(const tri_is &is, const float3 &ray_org, const float3 &ray_dir, const scene_refs *params) {
				uint32_t patch_ref = is.ref();
				bool upper = is.subd_quad_ref.is_upper_tri();
				int32_t subd_quad_ref = is.subd_quad_ref.ref();
				const float2 barycentrics = {.x = is.beta, .y = is.gamma};

				const subd_patch patch = params->patches[patch_ref];

				if (debug()) printf("Patch ref: %d, quad_ref: %d, upper: %d\n", patch_ref, subd_quad_ref, upper);

#ifndef BOX_APPROXIMATION
				const uint4 tri = patch.subd_tri(subd_quad_ref, upper);
				init_base(
					f3(params->patch_vertex_pos[tri.x]), f3(params->patch_vertex_pos[tri.y]), f3(params->patch_vertex_pos[tri.z]),
					f3(params->patch_vertex_norm[tri.x]), f3(params->patch_vertex_norm[tri.y]), f3(params->patch_vertex_norm[tri.z]),
					params->patch_vertex_tc[tri.x], params->patch_vertex_tc[tri.y], params->patch_vertex_tc[tri.z],
					barycentrics, &params->materials[tri.w]
				);
				if (debug()) printf("before: a: %d, b: %d, c: %d, material: %d\n", tri.x, tri.y, tri.z, tri.w);
				if (debug()) printf("before: TCs: %f %f\n", tc.x, tc.y);
				if (debug()) printf("before: TCs: %f %f %f\n", ns.x, ns.y, ns.z);
				return;
#else
				uint32_t vert_quad_ref = patch.quad_ref_from_index(subd_quad_ref); //REVIEW: only temp until TC and normal data is stored in subpatches
				const uint4 tri = patch.subd_tri(vert_quad_ref, upper); //REVIEW: still required here? or only for material?

				const subd_subpatch &subpatch = patch.subpatch_from_index(subd_quad_ref, params->subpatches);
				const mat3 &M = subpatch.trafo.transpose(); // equivalent to inverse here, REVIEW: base always orthogonal here? //TODO: check why this is not allowed without const?
				//float3 box_min, box_max;
				//subpatch.box_from_index(subd_quad_ref, params->patch_nodes, box_min, box_max);

				//float alpha = 1.f - barycentrics.x - barycentrics.y;
				//x  = alpha * a_pos  + barycentrics.x * b_pos  + barycentrics.y * c_pos;
				//ng = cross(b_pos-a_pos, c_pos-a_pos);
				x = ray_org + is.t * ray_dir;
				// TODO/REVIEW: adjust normal to the side of the box that has been hit
				// REVIEW: correct access to access column (and not row)?
				switch (is.subd_quad_ref.hit_side()) {
					case BOX_SIDE_FRONT:      ng = -M.read_vector(1); break;
					case BOX_SIDE_BACK:       ng = M.read_vector(1); break;
					case BOX_SIDE_SIDE_LEFT:  ng = cross(M.read_vector(0), M.read_vector(1)); break;
					case BOX_SIDE_SIDE_RIGHT: ng = cross(M.read_vector(1), M.read_vector(0)); break;
					case BOX_SIDE_SIDE_DOWN:  ng = cross(M.read_vector(2), M.read_vector(1)); break;
					case BOX_SIDE_SIDE_UP:    ng = cross(M.read_vector(1), M.read_vector(2)); break;
					default:                  ng = make_float3(0,0,0);
				}
				//normalize(ng); <- already normalized
				this->mat = &params->materials[tri.w];

				//REVIEW: put somewhere more suitable...
				float2 uv = patch.global_uvs(is.subd_quad_ref, is.beta, is.gamma);
				//assert(u >= 0 && u <= 1);
				//assert(v >= 0 && v <= 1);
				float2 u1 = patch.box_tcs[0] + (patch.box_tcs[1] - patch.box_tcs[0]) * uv.x;
				float2 u2 = patch.box_tcs[2] + (patch.box_tcs[3] - patch.box_tcs[2]) * uv.x;
				tc = u1 + (u2 - u1) * uv.y;

	#ifndef SHADE_BY_GEOMETRY_NORMAL
				//TODO: interpolate normal, REVIW: correct like that??
				float4 n1 = patch.box_norms[0] + (patch.box_norms[1] - patch.box_norms[0]) * uv.x;
				float4 n2 = patch.box_norms[2] + (patch.box_norms[3] - patch.box_norms[2]) * uv.x;
				ns = f3(n1 + (n2 - n1) * uv.y);
	#else
				ns = ng;
	#endif
				//if (debug()) printf("after: a: %d, b: %d, c: %d, material: %d\n", tri.x, tri.y, tri.z, tri.w);
				if (debug()) printf("after: TCs: %f %f\n", tc.x, tc.y);
				if (debug()) printf("ns: TCs: %f %f %f\n", ns.x, ns.y, ns.z);
				if (debug()) printf("pos x: %f %f %f\n", x.x, x.y, x.z);
				if (debug()) printf("bary: alpha: %f, beta %f, gamma %f\n", (1.f - barycentrics.x - barycentrics.y), barycentrics.x, barycentrics.y);
				if (debug()) printf("global UVs: u: %f, v: %f\n", uv.x, uv.y);
				//if (debug()) printf("a_pos: %f %f %f\n", a_pos.x, a_pos.y, a_pos.z);
				//if (debug()) printf("b_pos: %f %f %f\n", b_pos.x, b_pos.y, b_pos.z);
				//if (debug()) printf("c_pos: %f %f %f\n", c_pos.x, c_pos.y, c_pos.z);
				if (debug()) printf("box TCs 0: u: %f, v: %f\n", patch.box_tcs[0].x, patch.box_tcs[0].y);
				if (debug()) printf("box TCs 1: u: %f, v: %f\n", patch.box_tcs[1].x, patch.box_tcs[1].y);
				if (debug()) printf("box TCs 2: u: %f, v: %f\n", patch.box_tcs[2].x, patch.box_tcs[2].y);
				if (debug()) printf("box TCs 3: u: %f, v: %f\n", patch.box_tcs[3].x, patch.box_tcs[3].y);

	/*#ifdef PROJECTION
				// Project x back to oriented space
				const mat3 &M = subpatch.trafo.inverse();
				x = M * subpatch.projected_to_oriented(x);
	#endif*/
#endif

				// TODO: keep this assert?
				//assert(tc.x >= 0 && tc.x <= 1);
				//assert(tc.y >= 0 && tc.y <= 1);
			}
		};

		__device__ __forceinline__ bool not_black(float4 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}
		__device__ __forceinline__ bool not_black(float3 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}
		
		__device__ __forceinline__ float4 albedo4(const diff_geom &dg) {
			if (dg.mat->albedo_tex > 0) {
				return tex2D<float4>(dg.mat->albedo_tex, dg.tc.x, dg.tc.y);
			}
			return dg.mat->albedo;
		}

		__device__ __forceinline__ float3 albedo(const diff_geom &dg) {
			return f3(albedo4(dg));
		}

		__device__ __forceinline__ float4 emissive_albedo4(const diff_geom &dg) {
			float4 albedo = albedo4(dg);
			if(not_black(albedo))	return albedo * dg.mat->emissive;
			else					return dg.mat->emissive;
		}
	}
}
