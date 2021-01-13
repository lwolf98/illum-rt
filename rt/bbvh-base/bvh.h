#pragma once

#include "libgi/scene.h"
#include "libgi/intersect.h"

#include <vector>
#include <float.h>
#include <glm/glm.hpp>

// #define COUNT_HITS

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
	triangle_intersection closest_hit(const ray &ray) override;
	bool any_hit(const ray &ray) override;
};

enum class bbvh_triangle_layout { flat, indexed };
enum class bbvh_esc_mode { off, on };
template<bbvh_triangle_layout tr_layout, bbvh_esc_mode esc_mode>
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

	struct prim : public aabb {
		prim() : aabb() {}
		prim(const aabb &box, uint32_t tri_index) : aabb(box), tri_index(tri_index) {}
		vec3 center() const { return (min+max)*0.5f; }
		uint32_t tri_index;
	};

	template<bool cond, typename T>
    using variant = typename std::enable_if<cond, T>::type;

	std::vector<node> nodes;
	std::vector<uint32_t> index;  // can be empty if we don't use indexing
	
	enum binary_split_type {sm, om, sah};
	binary_split_type binary_split_type = om;
	
	int max_triangles_per_node = 1;

	int number_of_planes;
	uint32_t root;
	bool should_export = false;
	uint32_t max_depth;
	
	binary_bvh_tracer();
	void build(::scene *scene) override;
	triangle_intersection closest_hit(const ray &ray) override;
	bool any_hit(const ray &ray) override;
	bool interprete(const std::string &command, std::istringstream &in) override;

private:
	void early_split_clipping(std::vector<prim> &prims, std::vector<uint32_t> &index);
	uint32_t subdivide_om(std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end);
	uint32_t subdivide_sm(std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end);
	uint32_t subdivide_sah(std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end);
	void print_node_stats();
	void export_bvh(uint32_t node, uint32_t *id, uint32_t depth, std::string *filename);

	// Finalize BVH by (potentiall) replacing the scene triangles and (in any case) making a flat list
	template<bbvh_triangle_layout LO=tr_layout>
	variant<LO==bbvh_triangle_layout::flat,void>
		commit_shuffled_triangles(std::vector<prim> &prims, std::vector<uint32_t> &index);

	template<bbvh_triangle_layout LO=tr_layout> 
	variant<LO!=bbvh_triangle_layout::flat,void>
		commit_shuffled_triangles(std::vector<prim> &prims, std::vector<uint32_t> &index);

	// Get triangle index (if an index is used)
	template<bbvh_triangle_layout LO=tr_layout> 
	variant<LO==bbvh_triangle_layout::flat, int>
		triangle_index(int i) {
			return i;
		}
	
	template<bbvh_triangle_layout LO=tr_layout> 
	variant<LO!=bbvh_triangle_layout::flat, int>
		triangle_index(int i) {
			return index[i];
		}
};

