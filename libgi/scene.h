#pragma once

#include "camera.h"
#include "material.h"
#include "discrete_distributions.h"

#include "rt/bbvh-base/bvh.h"

#include <vector>
#include <map>
#include <string>
#include <filesystem>

struct texture {
	std::string name;
	std::filesystem::path path;
	unsigned w, h;
	vec3 *texel = nullptr;
	~texture() {
		delete [] texel;
	}
	const vec3& sample(float u, float v) const {
		u = u - floor(u);
		v = v - floor(v);
		int x = (int)(u*w+0.5f);
		int y = (int)(v*h+0.5f);
		if (x == w) x = 0;
		if (y == h) y = 0;
		return texel[y*w+x];
	}
	const vec3& sample(vec2 uv) const {
		return sample(uv.x, uv.y);
	}
	const vec3& operator()(float u, float v) const {
		return sample(u, v);
	}
	const vec3& operator()(vec2 uv) const {
		return sample(uv.x, uv.y);
	}
};

texture* load_image3f(const std::filesystem::path &path, bool crash_on_error = true);

struct light {
	virtual vec3 power() const = 0;
	virtual tuple<ray, vec3, float> sample_Li(const diff_geom &from, const vec2 &xis) const = 0;
// 	virtual float pdf(const ray &r) const = 0;
};

struct pointlight : public light {
	vec3 pos;
	vec3 col;
	pointlight(const vec3 pos, const vec3 col) : pos(pos), col(col) {}
	vec3 power() const override;
	tuple<ray, vec3, float> sample_Li(const diff_geom &from, const vec2 &xis) const override;
// 	float pdf(const ray &r) const override { return 0; }
};

/*! Keeping the emissive triangles as seperate copies might seem like a strange design choice.
 *  It is. However, this way the BVH is allowed to reshuffle triangle positions (not vertex positions!)
 *  without having an effect on this.
 *
 *  We might have to re-think this at a point, we could as well provide an indirection-array for the BVH's
 *  triangles. I opted against that at first, as not to intorduce overhead, but any more efficient representation
 *  copy the triangle data on the GPU or in SIMD formats anyway.
 */
struct trianglelight : public light, private triangle {
	const ::scene& scene;
	trianglelight(const ::scene &scene, uint32_t i);
	vec3 power() const override;
	tuple<ray, vec3, float> sample_Li(const diff_geom &from, const vec2 &xis) const override;
	float pdf(const ray &r, const diff_geom &on_light) const;
};

struct scene {
	struct object {
		std::string name;
		unsigned start, end;
		unsigned material_id;
	};
	std::vector<::vertex>    vertices;
	std::vector<::triangle>  triangles;
	std::vector<::material>  materials;
	std::vector<::texture*>  textures;
	std::vector<object>      objects;
	std::vector<object>      light_geom;
	std::vector<light*>      lights;
	std::map<std::string, ::camera> cameras;
	::camera camera;
	vec3 up;
	distribution_1d *light_distribution;
	void compute_light_distribution();
	const ::material material(uint32_t triangle_index) const {
		return materials[triangles[triangle_index].material_id];
	}
	scene() : camera(vec3(0,0,-1), vec3(0,0,0), vec3(0,1,0), 65, 1280, 720) {
	}
	~scene();
	void add(const std::filesystem::path &path, const std::string &name, const glm::mat4 &trafo = glm::mat4());

	vec3 normal(const triangle &tri) const;
	
	vec3 sample_texture(const triangle_intersection &is, const triangle &tri, const texture *tex) const;
	vec3 sample_texture(const triangle_intersection &is, const texture *tex) const {
		return sample_texture(is, triangles[is.ref], tex);
	}

	ray_tracer *rt = nullptr;
};

// std::vector<triangle> scene_triangles();
// scene load_scene(const std::string& filename);
