#pragma once

#include "scene.h"
#include "intersect.h"

#include <vector>
#include <float.h>
#include <glm/glm.hpp>

// #define COUNT_HITS

struct no_bvh : public ray_tracer {
	void build(::scene *scene) { this->scene = scene; }
	triangle_intersection closest_hit(const ray &ray);
};

struct naive_bvh : public ray_tracer {
	struct node {
		aabb box;
		uint32_t left, right;
		uint32_t triangle = (uint32_t)-1;
		bool inner() const { return triangle == (uint32_t)-1; }
	};

	std::vector<node> nodes;
	uint32_t root;
	void build(::scene *scene);
private:
	uint32_t subdivide(std::vector<triangle> &triangles, std::vector<vertex> &vertices, uint32_t start, uint32_t end);
	triangle_intersection closest_hit(const ray &ray);
};

struct binary_bvh_tracer : public ray_tracer {
	/* Innere und Blattknoten werden durch trickserei unterschieden.
	 * Für Blattknoten gilt:
	 * - link_l = -tri_offset
	 * - link_r = -tri_count
	 */
	struct node {
		aabb box_l, box_r;
		int32_t link_l, link_r;
		bool inner() const { return link_r >= 0; }
		int32_t tri_offset() const { return -link_l; }
		int32_t tri_count()  const { return -link_r; }
		void tri_offset(int32_t offset) { link_l = -offset; }
		void tri_count(int32_t count) { link_r = -count; }
	};

	std::vector<node> nodes;
	uint32_t root;
	binary_bvh_tracer();
	void build(::scene *scene) override;
	triangle_intersection closest_hit(const ray &ray) override;
	bool visible(const glm::vec3&) override { return false; }
private:
	uint32_t subdivide_om(std::vector<triangle> &triangles, std::vector<vertex> &vertices, uint32_t start, uint32_t end);
};

