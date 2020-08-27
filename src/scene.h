#pragma once

#include "cmdline.h"
#include "camera.h"
#include "bvh.h"

#include <vector>
#include <map>
#include <string>
#include <filesystem>

struct texture {
	std::string name;
	std::filesystem::path path;
	unsigned w, h;
	glm::vec3 *texel = nullptr;
	~texture() {
		delete [] texel;
	}
	const glm::vec3& operator()(float u, float v) const {
		u = u - floor(u);
		v = v - floor(v);
		int x = (int)(u*w+0.5f);
		int y = (int)(v*h+0.5f);
		if (x == w) x = 0;
		if (y == h) y = 0;
		return texel[y*w+x];
	}
	const glm::vec3& operator()(glm::vec2 uv) const {
		return (*this)(uv.x, uv.y);
	}
};

struct material {
	std::string name;
	glm::vec3 albedo = glm::vec3(0);
	texture *albedo_tex = nullptr;
};

struct scene {
	std::vector<::vertex>    vertices;
	std::vector<::triangle>  triangles;
	std::vector<::material>  materials;
	std::vector<::texture*>  textures;
	std::map<std::string, ::camera> cameras;
	::camera camera;
	glm::vec3 up;
	const ::material material(uint32_t triangle_index) const {
		return materials[triangles[triangle_index].material_id];
	}
	scene() : camera(glm::vec3(0,0,-1), glm::vec3(0,0,0), glm::vec3(0,1,0), 65, 1280, 720) {
	}
	~scene();
	void add(const std::filesystem::path &path, const std::string &name, const glm::mat4 &trafo = glm::mat4());

	glm::vec3 normal(const triangle &tri) const;
	
	glm::vec3 sample_texture(const triangle_intersection &is, const triangle &tri, const texture *tex) const;
	glm::vec3 sample_texture(const triangle_intersection &is, const texture *tex) const {
		return sample_texture(is, triangles[is.ref], tex);
	}

	ray_tracer *rt = nullptr;
};

// std::vector<triangle> scene_triangles();
// scene load_scene(const std::string& filename);
