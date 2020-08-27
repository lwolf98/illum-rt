#pragma once

#include <glm/glm.hpp>

struct ray {
	glm::vec3 o, d, id;
	ray(const glm::vec3 &o, const glm::vec3 &d) : o(o), d(d), id(1.0f/d.x, 1.0f/d.y, 1.0f/d.z) {}
	ray() {}
};

struct vertex {
	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 tc;
};

struct triangle {
	uint32_t a, b, c;
	uint32_t material_id;
};

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


class scene;

class ray_tracer {
protected:
	::scene *scene;
public:
	virtual void build(::scene *) = 0;
	virtual triangle_intersection closest_hit(const ray &) = 0;
	virtual bool visible(const glm::vec3 &) = 0;
	virtual ~ray_tracer() {}
};
