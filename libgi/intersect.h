#pragma once

#include "rt.h"

struct aabb {
	vec3 min, max;
	aabb() : min(FLT_MAX), max(-FLT_MAX) {}
	void grow(vec3 v) {
		min = glm::min(v, min);
		max = glm::max(v, max);
	}
	void grow(const aabb &other) {
		min = glm::min(other.min, min);
		max = glm::max(other.max, max);
	}
};

// See Shirley (2nd Ed.), pp. 206. (library or excerpt online)
inline bool intersect(const triangle &t, const vertex *vertices, const ray &ray, triangle_intersection &info) {
#ifndef RTGI_SKIP_RAY_TRI_IS_IMPL
	vec3 pos = vertices[t.a].pos;
	const float a_x = pos.x;
	const float a_y = pos.y;
	const float a_z = pos.z;

	pos = vertices[t.b].pos;
	const float &a = a_x - pos.x;
	const float &b = a_y - pos.y;
	const float &c = a_z - pos.z;
	
	pos = vertices[t.c].pos;
	const float &d = a_x - pos.x;
	const float &e = a_y - pos.y;
	const float &f = a_z - pos.z;
	
	const float &g = ray.d.x;
	const float &h = ray.d.y;
	const float &i = ray.d.z;
	
	const float &j = a_x - ray.o.x;
	const float &k = a_y - ray.o.y;
	const float &l = a_z - ray.o.z;

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

	if (tt > ray.t_min && tt < ray.t_max)
		if (beta > 0 && gamma > 0 && beta + gamma <= 1)
		{
			info.t = tt;
			info.beta = beta;
			info.gamma = gamma;
			return true;
		}

	return false;
#else
	// todo
	return false;
#endif
}	

#ifndef RTGI_SKIP_RAY_BOX_IS
inline bool intersect(const aabb &box, const ray &ray, float &is) {
#ifndef RTGI_SKIP_RAY_BOX_IS_1
	float t_near = -FLT_MAX;
	float t_far  =  FLT_MAX;

	if (ray.d.x == 0) {
		if (ray.o.x < box.min.x || ray.o.x > box.max.x)
			return false;
	}
	else {
		float t1 = (box.min.x - ray.o.x) / ray.d.x;
		float t2 = (box.max.x - ray.o.x) / ray.d.x;

		if (t1 > t2)	{	float tmp = t1;	t1 = t2; t2 = tmp; 	}

		if (t1 > t_near)	t_near = t1;
		if (t2 < t_far)		t_far = t2;

		if (t_near > t_far)	// box missed
			return false;

		if (t_far < ray.t_min || t_near > ray.t_max)
			return false;
	}

	if (ray.d.y == 0) {
		if (ray.o.y < box.min.y || ray.o.y > box.max.y)
			return false;
	}
	else {
		float t1 = (box.min.y - ray.o.y) / ray.d.y;
		float t2 = (box.max.y - ray.o.y) / ray.d.y;

		if (t1 > t2)	{	float tmp = t1;	t1 = t2; t2 = tmp; 	}

		if (t1 > t_near)	t_near = t1;
		if (t2 < t_far)		t_far = t2;

		if (t_near > t_far)	// box missed
			return false;

		if (t_far < ray.t_min || t_near > ray.t_max)
			return false;
	}

	if (ray.d.z == 0) {
		if (ray.o.z < box.min.z || ray.o.z > box.max.z)
			return false;
	}
	else {
		float t1 = (box.min.z - ray.o.z) / ray.d.z;
		float t2 = (box.max.z - ray.o.z) / ray.d.z;

		if (t1 > t2)	{	float tmp = t1;	t1 = t2; t2 = tmp; }

		if (t1 > t_near)	t_near = t1;
		if (t2 < t_far)		t_far = t2;

		if (t_near > t_far)	// box missed
			return false;

		if (t_far < ray.t_min || t_near > ray.t_max)
			return false;
	}

	is = t_near;
	return true;
#else
	// todo
	return false;
#endif
}

inline bool intersect2(const aabb &box, const ray &ray, float &is) {
#ifndef RTGI_SKIP_RAY_BOX_IS_2
		
	float t1x_tmp = (box.min.x - ray.o.x) / ray.d.x;
	float t2x_tmp = (box.max.x - ray.o.x) / ray.d.x;
	float t1x = (t1x_tmp < t2x_tmp) ? t1x_tmp : t2x_tmp;
	float t2x = (t2x_tmp < t1x_tmp) ? t1x_tmp : t2x_tmp;

	float t1y_tmp = (box.min.y - ray.o.y) / ray.d.y;
	float t2y_tmp = (box.max.y - ray.o.y) / ray.d.y;
	float t1y = (t1y_tmp < t2y_tmp) ? t1y_tmp : t2y_tmp;
	float t2y = (t2y_tmp < t1y_tmp) ? t1y_tmp : t2y_tmp;

	float t1z_tmp = (box.min.z - ray.o.z) / ray.d.z;
	float t2z_tmp = (box.max.z - ray.o.z) / ray.d.z;
	float t1z = (t1z_tmp < t2z_tmp) ? t1z_tmp : t2z_tmp;
	float t2z = (t2z_tmp < t1z_tmp) ? t1z_tmp : t2z_tmp;

	float t1 = (t1x < t1y) ? t1y : t1x;
	      t1 = (t1z < t1 ) ? t1  : t1z;
	float t2 = (t2x < t2y) ? t2x : t2y;
	      t2 = (t2z < t2 ) ? t2z : t2;
		
	if (t1 > t2)        return false;
	if (t2 < ray.t_min)	return false;
	if (t1 > ray.t_max) return false;
	
	is = t1;
	return true;
#else
	// todo
	return false;
#endif
}


inline bool intersect3(const aabb &box, const ray &ray, float &is) {
#ifndef RTGI_SKIP_RAY_BOX_IS_3
		
	float idx = 1.0f / ray.d.x;
	float idy = 1.0f / ray.d.y;
	float idz = 1.0f / ray.d.z;

	float t1x_tmp = (box.min.x - ray.o.x) * idx;
	float t2x_tmp = (box.max.x - ray.o.x) * idx;
	float t1x = (t1x_tmp < t2x_tmp) ? t1x_tmp : t2x_tmp;
	float t2x = (t2x_tmp < t1x_tmp) ? t1x_tmp : t2x_tmp;

	float t1y_tmp = (box.min.y - ray.o.y) * idy;
	float t2y_tmp = (box.max.y - ray.o.y) * idy;
	float t1y = (t1y_tmp < t2y_tmp) ? t1y_tmp : t2y_tmp;
	float t2y = (t2y_tmp < t1y_tmp) ? t1y_tmp : t2y_tmp;

	float t1z_tmp = (box.min.z - ray.o.z) * idz;
	float t2z_tmp = (box.max.z - ray.o.z) * idz;
	float t1z = (t1z_tmp < t2z_tmp) ? t1z_tmp : t2z_tmp;
	float t2z = (t2z_tmp < t1z_tmp) ? t1z_tmp : t2z_tmp;

	float t1 = (t1x < t1y) ? t1y : t1x;
	      t1 = (t1z < t1 ) ? t1  : t1z;
	float t2 = (t2x < t2y) ? t2x : t2y;
	      t2 = (t2z < t2 ) ? t2z : t2;
		
	if (t1 > t2)        return false;
	if (t2 < ray.t_min) return false;
	if (t1 > ray.t_max) return false;
	
	is = t1;
	return true;
#else
	// todo
	return false;
#endif
}

inline bool intersect4(const aabb &box, const ray &ray, float &is) {
#ifndef RTGI_SKIP_RAY_BOX_IS_4
		
	float idx = ray.id.x;
	float idy = ray.id.y;
	float idz = ray.id.z;

	float t1x_tmp = (box.min.x - ray.o.x) * idx;
	float t2x_tmp = (box.max.x - ray.o.x) * idx;
	float t1x = (t1x_tmp < t2x_tmp) ? t1x_tmp : t2x_tmp;
	float t2x = (t2x_tmp < t1x_tmp) ? t1x_tmp : t2x_tmp;

	float t1y_tmp = (box.min.y - ray.o.y) * idy;
	float t2y_tmp = (box.max.y - ray.o.y) * idy;
	float t1y = (t1y_tmp < t2y_tmp) ? t1y_tmp : t2y_tmp;
	float t2y = (t2y_tmp < t1y_tmp) ? t1y_tmp : t2y_tmp;

	float t1z_tmp = (box.min.z - ray.o.z) * idz;
	float t2z_tmp = (box.max.z - ray.o.z) * idz;
	float t1z = (t1z_tmp < t2z_tmp) ? t1z_tmp : t2z_tmp;
	float t2z = (t2z_tmp < t1z_tmp) ? t1z_tmp : t2z_tmp;

	float t1 = (t1x < t1y) ? t1y : t1x;
	      t1 = (t1z < t1 ) ? t1  : t1z;
	float t2 = (t2x < t2y) ? t2x : t2y;
	      t2 = (t2z < t2 ) ? t2z : t2;
		
	if (t1 > t2)        return false;
	if (t2 < ray.t_min) return false;
	if (t1 > ray.t_max) return false;
		
	is = t1;
	return true;
#else
	// todo
	return false;
#endif
}

#endif
