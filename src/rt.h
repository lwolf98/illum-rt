#pragma once

#include <glm/glm.hpp>
#include <utility>

using glm::vec3;
using glm::vec2;
using glm::vec4;
using std::pair;
using std::tuple;

struct ray {
	vec3 o, d, id;
	float max_t = FLT_MAX;
	ray(const vec3 &o, const vec3 &d) : o(o), d(d), id(1.0f/d.x, 1.0f/d.y, 1.0f/d.z) {}
	ray() {}
};

struct vertex {
	vec3 pos;
	vec3 norm;
	vec2 tc;
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
	vec3 barycentric_coord() const {
		vec3 bc;
		bc.x = 1.0 - beta - gamma;
		bc.y = beta;
		bc.z = gamma;
		return bc;
	}
};

class scene;
class material;

struct diff_geom {
	const vec3 x, ng, ns;
	const vec2 tc;
	const uint32_t tri;
	const material *mat;
	explicit diff_geom(const triangle_intersection &is, const scene &scene);

	vec3 albedo() const;
private:
	diff_geom(const vertex &a, const vertex &b, const vertex &c, const material *m, const triangle_intersection &is, const scene &scene);
	diff_geom(const triangle &tri, const triangle_intersection &is, const scene &scene);
};

class ray_tracer {
protected:
	::scene *scene;
public:
	virtual void build(::scene *) = 0;
	virtual triangle_intersection closest_hit(const ray &) = 0;
	virtual bool visible(const vec3 &) = 0;
	virtual ~ray_tracer() {}
};
