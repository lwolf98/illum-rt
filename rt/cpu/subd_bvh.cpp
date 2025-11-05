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
		if (triangles[start].material_id > scene->materials.size()-1) {
			// TODO: put in the SubD 2-level BVH node
			uint32_t patch_ref = ((uint32_t)-1) - triangles[start].material_id;
			subd::subd_patch &patch = scene->patches[patch_ref];

			subd::base_node copied_node;
			copied_node.box = patch.root_box;
			copied_node.set_secondary_value(patch_ref);
			copied_node.left = (uint32_t)-1;
			copied_node.right = (uint32_t)-1;
			nodes.emplace_back(copied_node);

			return nodes.size()-1;
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
	while (sp >= 0) {
		if (debug) std::cout << "\nStack pointer: " << sp << std::endl;
		if (debug) std::cout << "Node position: " << stack[sp] << std::endl;
		subd::base_node node = nodes[stack[sp--]];
#ifdef COUNT_HITS
		hits++;
#endif
		if (node.inner()) {
			float dist;
			if (debug) std::cout << "Regular node" << std::endl;
			if (intersect(node.box, ray, dist)) {
				if (dist < closest.t) {
					stack[++sp] = node.left;
					stack[++sp] = node.right;
					if (debug) std::cout << "Put two nodes on the stack!!!" << std::endl;
				}
				else
					if (debug) std::cout << "Not closer, not updated" << std::endl;
			}
			else
				if (debug) std::cout << "No intersect, not updated" << std::endl;
		}
		else {
			if (node.triangle > scene->triangles.size()-1) { // if leaf node holds a SubD quad:
				uint32_t patch_ref = node.get_secondary_value();
				traverse_patch(ray, patch_ref, closest);

				
			}
			else {
				if (debug) std::cout << "Regular leaf" << std::endl;
				if (intersect(scene->triangles[node.triangle], scene->vertices.data(), ray, intersection)) {
					if (intersection.t < closest.t) {
						closest = intersection;
						closest.ref = node.triangle;

						if (debug) std::cout << "Closest updated!!!" << std::endl;
					}
					else
						if (debug) std::cout << "Not closer, not updated" << std::endl;
				}
				else
					if (debug) std::cout << "No intersect, not updated" << std::endl;
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


//TODO: find better place for these functions
static uint32_t log2_clz(uint32_t x) {
	return 31 - __builtin_clz(x);
}

static uint32_t log4_clz(uint32_t x) {
	return (31 - __builtin_clz(x)) >> 1;
}

static int geometric_series4(int iterations) {
	return (1 - (1 << ((iterations+1)<<1))) / (-3);
}

/*static int geometric_series(int iterations, int base) {
	return (1-pow(base, iterations+1))/(1-base);
}*/

uint32_t child_node_base(
		uint32_t trav_level,
		uint32_t index
	) {
		uint32_t off_current_level = geometric_series4(trav_level-1);
		uint32_t off_child_level = geometric_series4(trav_level);
		uint32_t idx_current_relative = index - off_current_level;
		uint32_t idx_child_relative = idx_current_relative << 2; //(* 4)
		uint32_t index_child = off_child_level + idx_child_relative;
		return index_child;
}

uint32_t child_node_base(
		uint32_t index
	) {
		uint32_t trav_level = log4_clz(1+3*index);
		return child_node_base(trav_level, index);
}
//TODO end: until here

void subd_naive_bvh::traverse_patch(const ray &ray, uint32_t patch_ref, triangle_intersection &closest) {
	triangle_intersection intersection;
	const auto &patch = scene->patches[patch_ref];
	const auto &root_node = patch.nodes[0];

	// ---- REVIEW size
	//uint32_t max_size = patch.align_level + 4; // tree height + number of child nodes
	uint32_t max_size = 25;
	uint32_t stack[max_size];
	int32_t sp = 0;

	bool is_root_and_leaf = patch.align_level == 0;
	stack[sp] = 0; // If subd_level is 0, the stack/this value is not used

	//int32_t align_level = log2_clz(patch.trafos.size());

	while (sp >= 0) {
		uint32_t index = stack[sp--];
		uint32_t trav_level = log4_clz(1+3*index);

		bool is_leaf = trav_level == patch.align_level;
		if (!is_leaf) {
			const auto &node = patch.nodes[index];
			float dist;
			for (int i = 0; i < 4; ++i) {
				const aabb &box = node.boxes[i];
				//TODO: is it (more) efficient to not evaluate the last bounding box and instead evaluate the related quad/tris directly?
				if (intersect(box, ray, dist)) {
					if (dist < closest.t) {
						if (trav_level == 1)
							std::cout << "";
						uint32_t child_base = child_node_base(trav_level, index); //TODO: here or outside of loop?
						stack[++sp] = child_base+i;
					}
				}
			}
		}
		else {
			//uint32_t quad_ref = 0;
			uint32_t relative_index = 0;
			if (!is_root_and_leaf) {
				uint32_t off_current_level = geometric_series4(trav_level-1);
				relative_index = index - off_current_level;
				//quad_ref = patch.quad_ref_from_index(index - off_current_level, patch.align_level);
				//if (quad_ref > patch.subpatches.size()-1)
				//	std::cout << "";
				//if (patch.quad_ref_from_index(relative_index) > patch.subpatches.size()-1)
				//	std::cout << patch.quad_ref_from_index(relative_index) << std::endl;
			}

			if (patch.align_boxes) {
				uint32_t subpatch_index = patch.subpatch_ref_from_index(relative_index);
				traverse_subpatch(ray, patch.subpatches[subpatch_index], closest, patch);
			}
			else {
				uint32_t quad_ref = is_root_and_leaf ? 0 : quad_ref = patch.quad_ref_from_index(relative_index);
				assert(patch_ref >= 0);
				std::array<triangle, 2> tris = patch.tris(quad_ref);
				for (int i = 0; i < 2; i++) {
					if (intersect(tris[i], patch.verts.data(), ray, intersection)) {
						if (intersection.t < closest.t) {
							assert(quad_ref <= patch.verts.size());
							closest = intersection;
							closest.ref = ((uint32_t)-1) - patch_ref;
							closest.subd_quad_ref = quad_ref + 1;
							if (i == 1)
								closest.subd_quad_ref *= -1;

							break; // TODO: This should always be correct, right? Should not be possible to hit both tris...
						}
					}
				}	
			}
		}
	}
}

void subd_naive_bvh::traverse_subpatch(const ray &rayy, const subd::subd_subpatch &subpatch, triangle_intersection &closest, const subd::subd_patch &dbg_patch) {
	/**
	 * root_node = subpatch.nodes[0]
	 */

	triangle_intersection intersection;
	//const auto &patch = *subpatch.parent; //scene->patches[patch_ref];
	const auto &patch = dbg_patch;
	const auto &root_node = subpatch.nodes[0];
	assert(&patch == &dbg_patch);
	//assert(subpatch.parent == &dbg_patch);

	// ---- REVIEW size
	//uint32_t max_size = subpatch.subd_level + 4; // tree height + number of child nodes
	uint32_t max_size = 25;
	uint32_t stack[max_size];
	int32_t sp = 0;

	//REVIEW: check this case:
	bool is_root_and_leaf = subpatch.subd_level == 0;
	stack[sp] = 0; // If subd_level is 0, the stack/this value is not used

	//int32_t align_level = log2_clz(patch.trafos.size());

	/*ray transformed_ray = ray(
						inverse(subpatch.trafo) * rayy.o,
						inverse(subpatch.trafo) * rayy.d
					);*/
	ray transformed_ray = ray(
						subpatch.trafo * rayy.o,
						subpatch.trafo * rayy.d
					);

	while (sp >= 0) {
		uint32_t index = stack[sp--];
		uint32_t trav_level = log4_clz(1+3*index);

		bool is_leaf = trav_level == subpatch.subd_level;
		if (!is_leaf) {
			const auto &node = subpatch.nodes[index];
			float dist;
			for (int i = 0; i < 4; ++i) {
				const aabb &box = node.boxes[i];
				//TODO: is it (more) efficient to not evaluate the last bounding box and instead evaluate the related quad/tris directly?
				if (intersect(box, transformed_ray, dist)) {
					if (dist < closest.t) {
						uint32_t child_base = child_node_base(trav_level, index); //TODO: here or outside of loop?
						stack[++sp] = child_base+i;
					}
				}
			}
		}
		else {
			uint32_t quad_ref = 0;
			if (!is_root_and_leaf) {
				uint32_t off_current_level = geometric_series4(trav_level-1);
				uint32_t relative_index = index - off_current_level;
				//quad_ref = patch.quad_ref_from_index(index - off_current_level);
				//uint32_t quad_index = subpatch.vert_start + relative_index;
				//quad_ref = patch.quad_ref_from_index(quad_index);
				quad_ref = subpatch.vert_start + patch.quad_ref_from_index(relative_index);
				if (quad_ref > 19)
					std::cout << "";
				if (relative_index == 3)
					std::cout << "";
				if (subpatch.vert_start == 2)
					std::cout << "";
			}

			//---- REVIEW: assert(patch_ref >= 0);
			std::array<triangle, 2> tris = patch.tris(quad_ref);
			for (int i = 0; i < 2; i++) {
				if (intersect(tris[i], patch.verts.data(), rayy, intersection)) {
					if (intersection.t < closest.t) {
						assert(quad_ref <= patch.verts.size());
						closest = intersection;
						//---- REVIEW: closest.ref = ((uint32_t)-1) - patch_ref;
						closest.subd_quad_ref = quad_ref + 1;
						if (i == 1)
							closest.subd_quad_ref *= -1;

						break; // TODO: This should always be correct, right? Should not be possible to hit both tris...
					}
				}
			}
		}
	}
}
