#pragma once

#include "rt.h"
#include "scene.h"

struct triangle_intersection {
	typedef unsigned int uint;
	float t, beta, gamma;
	uint ref;
	triangle_intersection() : t(FLT_MAX), ref(0) {
	}
	triangle_intersection(uint t) : t(FLT_MAX), ref(t) {
	}
	bool valid() const {
		return t != FLT_MAX;
	}
	void reset() {
		t = FLT_MAX;
		ref = 0;
	}
	glm::vec3 barycentric_coord() const {
		glm::vec3 bc;
		bc.x = 1.0 - beta - gamma;
		bc.y = beta;
		bc.z = gamma;
		return bc;
	}
};


// Siehe Shirley (2nd Ed.), 206ff.
inline bool intersect(const triangle &t, const vertex *vertices, const ray &ray, triangle_intersection &info) {
	glm::vec3 pos = vertices[t.a].pos;
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

	if (tt > 0)
		if (beta > 0 && gamma > 0 && beta + gamma <= 1)
		{
			info.t = tt;
			info.beta = beta;
			info.gamma = gamma;
			return true;
		}

	return false;
}	

