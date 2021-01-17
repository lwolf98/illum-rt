#pragma once

#include "camera.h"
#include "intersect.h"
#include "material.h"
#ifndef RTGI_A05
#include "discrete_distributions.h"
#endif

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
	const vec3& value(int x, int y) const {
		return texel[y*w+x];
	}
	const vec3& operator[](glm::uvec2 xy) const {
		return value(xy.x, xy.y);
	}
};

texture* load_image3f(const std::filesystem::path &path, bool crash_on_error = true);
texture* load_hdr_image3f(const std::filesystem::path &path);

#ifndef RTGI_A02
struct light {
	virtual ~light() {}
	virtual vec3 power() const = 0;
#if !(defined(RTGI_A05) || defined(RTGI_A05_REF))
	virtual tuple<ray, vec3, float> sample_Li(const diff_geom &from, const vec2 &xis) const = 0;
// 	virtual float pdf(const ray &r) const = 0;
#endif
};

struct pointlight : public light {
	vec3 pos;
	vec3 col;
	pointlight(const vec3 pos, const vec3 col) : pos(pos), col(col) {}
	vec3 power() const override;
#if !(defined(RTGI_A05) || defined(RTGI_A05_REF))
	tuple<ray, vec3, float> sample_Li(const diff_geom &from, const vec2 &xis) const override;
// 	float pdf(const ray &r) const override { return 0; }
#endif
};
#endif

#ifndef RTGI_A04 
/*! Keeping the emissive triangles as seperate copies might seem like a strange design choice.
 *  It is. However, this way the BVH is allowed to reshuffle triangle positions (not vertex positions!)
 *  without having an effect on this.
 *
 *  We might have to re-think this at a point, we could as well provide an indirection-array for the BVH's
 *  triangles. I opted against that at first, as not to intorduce overhead, but any more efficient representation
 *  copy the triangle data on the GPU or in SIMD formats anyway.
 */
#endif

#if defined(RTGI_A05) || defined(RTGI_A05_REF)
struct trianglelight : public light, public triangle {
	const ::scene& scene;
	trianglelight(const ::scene &scene, uint32_t i);
	vec3 power() const override;
};
#endif
#if !(defined(RTGI_A05) || defined(RTGI_A05_REF))
struct trianglelight : public light, private triangle {
	const ::scene& scene;
	trianglelight(const ::scene &scene, uint32_t i);
	vec3 power() const override;
	tuple<ray, vec3, float> sample_Li(const diff_geom &from, const vec2 &xis) const override;
	float pdf(const ray &r, const diff_geom &on_light) const;
};
#endif

#ifndef RTGI_AXX
struct skylight : public light {
	texture *tex = nullptr;
	float intensity_scale;
	distribution_2d *distribution = nullptr;
	float scene_radius;

	skylight(const std::filesystem::path &file, float intensity_scale) : intensity_scale(intensity_scale) {
		tex = load_hdr_image3f(file);
	}
	void build_distribution();
	void scene_bounds(aabb box);
	vec3 Le(const ray &ray) const;
	virtual vec3 power() const;
	virtual tuple<ray, vec3, float> sample_Li(const diff_geom &from, const vec2 &xis) const;
	float pdf_Li(const ray &ray) const;
};

#endif

/*  \brief The scene culminates all the geometric information that we use.
 *
 *  This naturally includes the surface geometry to be displayed, but also light sources and cameras.
 *
 *  The scene also holds a reference to the ray tracer as the tracer runs on the scene's data.
 *
 */
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
#ifndef RTGI_A02
	std::map<std::string, brdf*> brdfs;
	std::vector<light*>      lights;
#endif
#ifndef RTGI_A04
	std::vector<object>      light_geom;	// Expires after bvh is built, do not use!
	void compute_light_distribution();
#endif
#ifndef RTGI_A05
	distribution_1d *light_distribution;
#endif
#ifndef RTGI_AXX
	skylight *sky = nullptr;
#endif
	std::map<std::string, ::camera> cameras;
	::camera camera;
	vec3 up;
	aabb scene_bounds;
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
