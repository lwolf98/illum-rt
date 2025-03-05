#include "subd_bvh.h"

#include <algorithm>
#include <iostream>
#include <chrono>

using namespace glm;

// 
//    subd naive_bvh
//

void subd_naive_bvh::build(::scene *scene) {
	this->scene = scene;
	std::cout << "Building BVH..." << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();

	root = subdivide(scene->triangles, scene->vertices, 0, scene->triangles.size());
	
	auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	std::cout << "Done after " << duration << "ms" << std::endl;
}

uint32_t subd_naive_bvh::subdivide(std::vector<triangle> &triangles, std::vector<vertex> &vertices, uint32_t start, uint32_t end) {
#ifndef RTGI_SKIP_BVH1_OM_IMPL
	assert(start < end);

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end - start == 1) {
		//if (triangles[start].material_id == (uint32_t)-1) {
		if (triangles[start].material_id > scene->materials.size()-1) {
			// TODO: put in the SubD 2-level BVH node
			uint32_t patch_ref = ((uint32_t)-1) - triangles[start].material_id;
			subd::subd_patch &patch = scene->patches[patch_ref];

			// TODO:
			// add patch nodes to this list
			// update id by offset

			uint32_t offset = nodes.size();
			for (auto &node : patch.nodes) {
				nodes.emplace_back(node);
				auto &copied_node = nodes[nodes.size()-1];
				if (copied_node.left != ((uint32_t)-1))
					copied_node.left += offset;
				if (copied_node.right != ((uint32_t)-1))
					copied_node.right += offset;
			}
			uint32_t id = patch.bvh_node + offset;

			//nodes[id].triangle = start;
			return id;
		}
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
#else
	// todo
	throw std::logic_error("Not implemented, yet");
	return 0;
#endif
}

triangle_intersection subd_naive_bvh::closest_hit(const ray &ray) {
#ifndef RTGI_SKIP_BVH1_TRAV_IMPL
	triangle_intersection closest, intersection;
	uint32_t stack[25];
	int32_t sp = 0;
	stack[0] = root;
#ifdef COUNT_HITS
	unsigned int hits = 0;
#endif
	int patch_ref = -1;
	while (sp >= 0) {
		subd::node node = nodes[stack[sp--]];
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
			if (node.triangle > scene->triangles.size()-1) { // if leaf node holds a SubD quad:
				// TODO: evaluate SubD quad
				// triangle tri1 = quad.tri1();
				// triangle tri2 = quad.tri2();
				// make intersect test from below for both tris...
				//uint32_t morton_code = ((uint32_t)-1) - node.triangle;
				if (node.left != ((uint32_t)-1) || node.right != ((uint32_t)-1)) {
					// branch: hit subd patch root node
					patch_ref = node.get_secondary_value();
					float dist;
					if (intersect(node.box, ray, dist)) {
						if (dist < closest.t) {
							stack[++sp] = node.left;
							stack[++sp] = node.right;
						}
					}
				}
				else {
					// branch: hit subd patch leaf node (subd quad)
					uint32_t morton_code = node.get_secondary_value();
					assert(patch_ref >= 0);
					subd::subd_patch &patch = scene->patches[patch_ref];
					std::array<triangle, 2> tris = patch.tris(morton_code);
					for (int i = 0; i < 2; i++) {
						//if (intersect(scene->patches[patch_ref], morton_code, )) {
						if (intersect(tris[i], patch.verts.data(), ray, intersection)) {
							if (intersection.t < closest.t) {
								closest = intersection;
								closest.ref = ((uint32_t)-1) - patch_ref;
								closest.subd_quad_ref = morton_code + 1;
								if (i == 1)
									closest.subd_quad_ref *= -1;

								break; // TODO: This should always be correct, right? Should not be possible to hit both tris...
							}
						}
					}
				}
			}
			else {
				if (intersect(scene->triangles[node.triangle], scene->vertices.data(), ray, intersection)) {
					if (intersection.t < closest.t) {
						closest = intersection;
						closest.ref = node.triangle;
					}
				}
			}
		}
	}
#ifdef COUNT_HITS
	closest.ref = hits;
#endif
	return closest;
#else
	// todo
	throw std::logic_error("Not implemented, yet");
	return triangle_intersection();
#endif
}


bool subd_naive_bvh::any_hit(const ray &ray) {
	auto is = closest_hit(ray);
	if (is.valid())
		return true;
	return false;
}

