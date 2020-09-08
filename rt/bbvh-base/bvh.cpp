#include "bvh.h"

#include <algorithm>
#include <iostream>
#include <chrono>

using namespace glm;

// 
//    no_bvh
//

triangle_intersection no_bvh::closest_hit(const ray &ray) {
	triangle_intersection closest, intersection;
	for (int i = 0; i < scene->triangles.size(); ++i)
		if (intersect(scene->triangles[i], scene->vertices.data(), ray, intersection))
			if (intersection.t < closest.t) {
				closest = intersection;
				closest.ref = i;
			}
	return closest;
}

// 
//    naive_bvh
//

void naive_bvh::build(::scene *scene) {
	this->scene = scene;
	std::cout << "Building BVH..." << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();

	root = subdivide(scene->triangles, scene->vertices, 0, scene->triangles.size());
    
	auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	std::cout << "Done after " << duration << "ms" << std::endl;
}

uint32_t naive_bvh::subdivide(std::vector<triangle> &triangles, std::vector<vertex> &vertices, uint32_t start, uint32_t end) {
	assert(start < end);

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end - start == 1) {
		uint32_t id = nodes.size();
		nodes.emplace_back();
		nodes[id].triangle = start;
		return id;
	}

	// Hilfsfunktionen
	auto bounding_box = [&](const triangle &triangle) {
		aabb box;
		box.grow(vertices[triangle.a].pos);
		box.grow(vertices[triangle.b].pos);
		box.grow(vertices[triangle.c].pos);
		return box;
	};
	auto center = [&](const triangle &triangle) {
		return (vertices[triangle.a].pos +
				vertices[triangle.b].pos +
				vertices[triangle.c].pos) * 0.333333f;
	};

	// Bestimmen der Bounding Box der (Teil-)Szene
	aabb box;
	for (int i = start; i < end; ++i)
		box.grow(bounding_box(triangles[i]));

	// Sortieren nach der größten Achse
	vec3 extent = box.max - box.min;
	float largest = max(extent.x, max(extent.y, extent.z));
	if (largest == extent.x)
		std::sort(triangles.data()+start, triangles.data()+end,
				  [&](const triangle &a, const triangle &b) { return center(a).x < center(b).x; });
	else if (largest == extent.y)
		std::sort(triangles.data()+start, triangles.data()+end,
				  [&](const triangle &a, const triangle &b) { return center(a).y < center(b).y; });
	else 
		std::sort(triangles.data()+start, triangles.data()+end,
				  [&](const triangle &a, const triangle &b) { return center(a).z < center(b).z; });

	// In der Mitte zerteilen
	int mid = start + (end-start)/2;
	uint32_t id = nodes.size();
	nodes.emplace_back();
	uint32_t l = subdivide(triangles, vertices, start, mid);
	uint32_t r = subdivide(triangles, vertices, mid,   end);
	nodes[id].left  = l;
	nodes[id].right = r;
	nodes[id].box = box;
	return id;
}

triangle_intersection naive_bvh::closest_hit(const ray &ray) {
	triangle_intersection closest, intersection;
	uint32_t stack[25];
	int32_t sp = 0;
	stack[0] = root;
#ifdef COUNT_HITS
	unsigned int hits = 0;
#endif
	while (sp >= 0) {
		node node = nodes[stack[sp--]];
#ifdef COUNT_HITS
		hits++;
#endif
		if (node.inner()) {
			float dist;
			if (intersect(node.box, ray, dist))
				if (dist < closest.t) {
					stack[++sp] = node.left;
					stack[++sp] = node.right;
				}
		}
		else {
			if (intersect(scene->triangles[node.triangle], scene->vertices.data(), ray, intersection))
				if (intersection.t < closest.t) {
					closest = intersection;
					closest.ref = node.triangle;
				}
		}
	}
#ifdef COUNT_HITS
	closest.ref = hits;
#endif
	return closest;
}
