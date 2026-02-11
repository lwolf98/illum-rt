#include "subd_bvh.h"
#include "debug/pixel.h"

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

void subd_naive_bvh::traverse_patch(const ray &ray, uint32_t patch_ref, triangle_intersection &closest) {
	using namespace subd;

	triangle_intersection intersection;
	const auto &patch = scene->patches[patch_ref];

	// ---- REVIEW size
	//uint32_t max_size = patch.align_level + 4; // tree height + number of child nodes
	uint32_t max_size = 25;
	uint32_t stack[max_size];
	int32_t sp = 0;

	bool is_root_and_leaf = patch.align_level == 0;
	stack[sp] = 0; // If subd_level is 0, the stack/this value is not used

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
						uint32_t child_base = child_node_base(trav_level, index); //TODO: here or outside of loop?
						stack[++sp] = child_base+i;
					}
				}
			}
		}
		else {
			uint32_t relative_index = 0;
			if (!is_root_and_leaf) {
				uint32_t off_current_level = geometric_series4(trav_level-1);
				relative_index = index - off_current_level;
			}

			if (patch.align_boxes) {
				traverse_subpatch(ray, patch.subpatches[relative_index], closest, patch_ref);
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

							closest.subd_quad_ref.set_ref(quad_ref);
							closest.subd_quad_ref.set_upper_tri(i == 0);

							break; // TODO: This should always be correct, right? Should not be possible to hit both tris...
						}
					}
				}	
			}
		}
	}
}

inline bool dbg_pixel() {
	bool dbg = true;
	return dbg && current_pixel_x == debug_pixel_x && current_pixel_y == debug_pixel_y;
}

inline void bary_calc(const aabb &box, const ray &ray, float t_dist, triangle_intersection &is) {
	//const float eps = 1e-4f;

	vec3 hit = ray.o + t_dist * ray.d;
	float width = box.max.x - box.min.x;
	float height = box.max.z - box.min.z;
	vec3 hit_relative = hit - box.min;
	vec2 hit_xy(hit_relative.x, hit_relative.z);
	hit_xy.x = hit_xy.x / width;
	hit_xy.y = hit_xy.y / height;
	//assert(hit_xy.x >= -eps && hit_xy.x <= 1+eps);
	//assert(hit_xy.y >= -eps && hit_xy.y <= 1+eps);
	if (hit_xy.x < 0) hit_xy.x = 0;
	if (hit_xy.x > 1) hit_xy.x = 1;
	if (hit_xy.y < 0) hit_xy.y = 0;
	if (hit_xy.y > 1) hit_xy.y = 1;

	bool upper_tri = hit_xy.y < -hit_xy.x + 1;
	is.subd_quad_ref.set_upper_tri(upper_tri);
	if (upper_tri) {
		is.beta = hit_xy.x;
		is.gamma = hit_xy.y;
	}
	else {
		is.beta = 1 - hit_xy.x;
		is.gamma = 1 - hit_xy.y;
	}
	assert(is.beta >= 0 && is.beta <= 1);
	assert(is.gamma >= 0 && is.gamma <= 1);
	assert(is.beta + is.gamma >= 0);
	assert(is.beta + is.gamma <= 1);
}

inline bool compute_valid_hit(
	const aabb &box,
	const ray &transformed_ray,
	float closest_t,
#ifdef PROJECTION
	float t_near_oriented,
	const glm::vec3 &hit_near_oriented,
	float t_off,
	const subd::subd_subpatch &subpatch,
#endif
	bool allow_negative_t,
	bool intersect_box_mid,
	float &t_hit,
	float &t_bary,
	float &t_mid_box,
	uint32_t &hit_side
) {
	float dist;
	t_mid_box = 0.f;

	//if (!intersect4(box, transformed_ray, dist)) return false;
	if (!intersect4_extended(box, transformed_ray, dist, hit_side)) return false;
#ifdef BOX_MID_INTERSECTION
	#if defined(BOX_MID_VAR_CARDBOX) && !defined(BOX_MID_VAR_PROJECTION)
		intersect_box_mid = intersect_box_mid && (hit_side == BOX_SIDE_FRONT || hit_side == BOX_SIDE_BACK);
	#endif
	if (intersect_box_mid) {
		if (dbg_pixel())
			std::cout << "DBG: Primary hit side: " << hit_side << std::endl;

		//uint32_t hit_side_mid;
		//float dist_mid;
		aabb mid_box = box;
		float mid = box.min.y + (box.max.y - box.min.y)*0.5f;
	#ifdef BOX_MID_VAR_FLAT
		// Flat Variant
		constexpr float eps = 1e-6; //TODO: switch this IS with quad intersection, then no eps required
		mid_box.min.y = mid;
		mid_box.max.y = mid+eps;
		if (!intersect4_extended(mid_box, transformed_ray, dist, hit_side)) return false;
	#else
		// Cardboard Box Variant

		//mid_box.min.y = box.min.y - 1;
		mid_box.max.y = mid;

		//mid_box.min.y = mid;

		//vec3 inside_org = transformed_ray.o + dist * transformed_ray.d;
		//ray inside_ray(inside_org, transformed_ray.d);
		ray inside_ray = transformed_ray;
		//ray inside_ray = ray(transformed_ray.o, transformed_ray.d);
		inside_ray.t_min += dist;
		bool hit_front = true;
		//if (!intersect4_mid_box(mid_box, inside_ray, dist, hit_side)) return false;
		if (!intersect4_mid_box(mid_box, inside_ray, dist, hit_side)) {
			#ifdef BOX_MID_SUPPORT_BACK_SIDE
				mid_box.max.y = box.max.y;
				mid_box.min.y = mid;
				if (!intersect4_mid_box(mid_box, inside_ray, dist, hit_side)) return false;
				hit_front = false;
			#else
				return false;
			#endif
		}
		#ifdef BOX_MID_VAR_PROJECTION
			// Projection Variant
			float mid_box_hit_y = transformed_ray.o.y + dist * transformed_ray.d.y;
			t_mid_box = hit_front ? mid_box_hit_y - mid : mid - mid_box_hit_y;
			hit_side = hit_front ? BOX_SIDE_FRONT : BOX_SIDE_BACK;
		#endif
	#endif
		//hit_side = hit_side_mid;
		//if (hit_side == BOX_SIDE_FRONT) return false;
	}
#endif

#ifndef PROJECTION
	//assert(!std::isnan(dist)); // REVIEW: can this happen?
	if (std::isnan(dist)) return false;
	if (dist >= closest_t) return false;
	if (!allow_negative_t && dist <= 0) return false;
	t_hit = dist;
	t_bary = dist;
#else
	dist += t_off; // correct by earlier added epsilon

	// Calculate new t
	float t_total;
	vec3 x_proj = transformed_ray.o + dist * transformed_ray.d;
	vec3 x_oriented = subpatch.projected_to_oriented(x_proj);
	float t_dist = glm::length(x_oriented - hit_near_oriented);
	t_total = t_near_oriented + t_dist;
		
	if (isnan(t_total)) return false; //REVIEW: check nans here
	if (t_total >= closest_t) return false; // -> fixes overlapping and shadow ray self intersection
	if (!allow_negative_t && t_total <= 0) return false;
	t_hit = t_total;
	t_bary = dist;
#endif

	return true;
}

void subd_naive_bvh::traverse_subpatch(const ray &rayy, const subd::subd_subpatch &subpatch, triangle_intersection &closest, uint32_t patch_ref) {
	using namespace subd;
	//constexpr bool intersect_box_mid = true;

	bool dbg_px = dbg_pixel();
	//bool dbg_pixel = true;
	//dbg_pixel = dbg_pixel && current_pixel_x == debug_pixel_x && current_pixel_y == debug_pixel_y;
	if (dbg_px)
		std::cout << "";

	triangle_intersection intersection;
	const auto &patch = scene->patches[patch_ref];

	// ---- REVIEW size
	//uint32_t max_size = subpatch.subd_level + 4; // tree height + number of child nodes
	constexpr uint32_t max_size = 25;
	uint32_t stack[max_size];
	aabb box_stack[max_size];
	int32_t sp = 0;

	bool is_root_and_leaf = subpatch.subd_level == 0;
	stack[sp] = 0; // If subd_level is 0, the stack/this value is not used
	box_stack[sp] = subpatch.root_box;

	ray transformed_ray = ray(
						subpatch.trafo * rayy.o,
						subpatch.trafo * rayy.d
					);
#ifndef PROJECTION
	transformed_ray.t_min = rayy.t_min; // TODO: test this in Box approximation !
	transformed_ray.t_max = rayy.t_max;
#else
	const float eps = transformed_ray.eps;

	// Note: root_box is in projected space, but the y coordinate can also be used to
	// calculate points in oriented space here. x and z cannot directly be mapped.
	float t1 = (subpatch.root_box.max.y - transformed_ray.o.y) * transformed_ray.id.y;
	float t2 = (subpatch.root_box.min.y - transformed_ray.o.y) * transformed_ray.id.y;
	if (t1 > t2) std::swap(t1, t2);

	vec3 p1_oriented = transformed_ray.o + t1 * transformed_ray.d;
	vec3 p1 = subpatch.oriented_to_projected(p1_oriented);
	vec3 p2 = subpatch.oriented_to_projected(transformed_ray.o + t2 * transformed_ray.d);
	
	vec3 dir = (t1 != t2) ? normalize(p2-p1) : vec3(0, 1.f, 0); // REVIEW: stable solution for t1 == t2?
	p1 = p1 - eps * dir; // -> eps offset fixes (in this case wanted) potential self intersections
	transformed_ray = ray(p1, dir);
	//transformed_ray.t_min = 0; // REVIEW: correct here? Or how to translate: rayy.t_min ?
	//transformed_ray.t_max = FLT_MAX; // REVIEW: correct here? Or how to translate: rayy.t_max ?
	//transformed_ray.t_max = 2.f; // -> quick fix for artifacts, but does not explain them...
#endif

	while (sp >= 0) {
		uint32_t index = stack[sp];
		const aabb &parent_box = box_stack[sp];
		sp--;

		uint32_t trav_level = log4_clz(1+3*index);
		bool is_leaf = trav_level == subpatch.subd_level;
		if (!is_leaf) {
			const auto &node = subpatch.nodes[index];
			uint32_t child_base = child_node_base(trav_level, index);
			uint32_t off_current_level = geometric_series4(trav_level);
			bool is_leaf_node = trav_level >= subpatch.subd_level-1;
			bool intersect_box_mid = def_intersect_box_mid && is_leaf_node;
			float dist;
			for (int i = 0; i < 4; ++i) {
#if !defined(SLAB_COMPRESSION) && !defined(QUANTIZATION)
				const aabb &box = node.boxes[i];
#else
	#if !defined(QUANTIZATION) && !defined(HALF_SLAB_COMPRESSION)
				const aabb box = node.get_box<aabb>(i);
	#else
				const aabb box = node.get_box(i, parent_box);
	#endif
#endif
				//TODO: is it (more) efficient to not evaluate the last bounding box and instead evaluate the related quad/tris directly?
				float t_hit, t_bary, t_mid_box;
				uint32_t hit_side; //REVIEW/TODO: optimization: only use intersect_exteded (in compute_valid_hit) when trav_level == subpatch.subd_level-1
#ifndef PROJECTION
				if (!compute_valid_hit(box, transformed_ray, closest.t, true, intersect_box_mid, t_hit, t_bary, t_mid_box, hit_side)) continue;
#else
				if (!compute_valid_hit(box, transformed_ray, closest.t, t1, p1_oriented, eps, subpatch, false, intersect_box_mid, t_hit, t_bary, t_mid_box, hit_side)) continue;
#endif

#ifndef BOX_APPROXIMATION
				stack[++sp] = child_base+i;
#else
				if (trav_level < subpatch.subd_level-1) {
					sp++;
					stack[sp] = child_base+i;
					box_stack[sp] = box;
				}
				else {
	#ifndef PROJECTION
					if (t_hit <= 0) continue;
	#endif
					bary_calc(box, transformed_ray, t_bary, closest); // !REVIEW: breaks for quantisation without slab compression on CPU, check what is happening
					closest.ref = ((uint32_t)-1) - patch_ref;
					closest.t = t_hit;
					closest.t_mid_box = t_mid_box;

					uint32_t relative_index = (child_base+i) - off_current_level;
					uint32_t quad_ref_morton =    patch.index_from_quad_ref(subpatch.vert_start)
												+ relative_index;

					closest.subd_quad_ref.set_ref(quad_ref_morton);
					closest.subd_quad_ref.set_hit_side(hit_side);
				}
#endif
			}
		}
		else {
#ifndef BOX_APPROXIMATION
			/* Regular geometry hit */
			uint32_t quad_ref = subpatch.vert_start;
			if (!is_root_and_leaf) {
				uint32_t off_current_level = geometric_series4(trav_level-1);
				uint32_t relative_index = index - off_current_level;
				quad_ref += patch.quad_ref_from_index(relative_index);
			}

			// TODO: ! supply projection logic !
			std::array<triangle, 2> tris = patch.tris(quad_ref);
			for (int i = 0; i < 2; i++) {
				if (intersect(tris[i], patch.verts.data(), rayy, intersection)) {
					if (intersection.t < closest.t) {
						assert(quad_ref <= patch.verts.size());
						closest = intersection;
						closest.ref = ((uint32_t)-1) - patch_ref;

						closest.subd_quad_ref.set_ref(quad_ref);
						closest.subd_quad_ref.set_upper_tri(i == 0);

						break; // TODO: This should always be correct, right? Should not be possible to hit both tris...
					}
				}
			}
#else
			/* Box approximation root box hit */
			float t_hit, t_bary, t_mid_box;
			uint32_t hit_side; //REVIEW/TODO: optimization: only use intersect_exteded (in compute_valid_hit) when trav_level == subpatch.subd_level-1
			
#ifndef PROJECTION
			// REVIEW: Before using this function, dist < closest.t was not tested and slightly other results. Which is correct?
			if (!compute_valid_hit(subpatch.root_box, transformed_ray, closest.t, false, def_intersect_box_mid, t_hit, t_bary, t_mid_box, hit_side)) continue;
#else
			if (!compute_valid_hit(subpatch.root_box, transformed_ray, closest.t, t1, p1_oriented, eps, subpatch, false, def_intersect_box_mid, t_hit, t_bary, t_mid_box, hit_side)) continue;
#endif

				bary_calc(subpatch.root_box, transformed_ray, t_bary, closest);
				closest.t = t_hit;
				closest.ref = ((uint32_t)-1) - patch_ref;
				closest.t_mid_box = t_mid_box;

				uint32_t quad_ref_morton = patch.index_from_quad_ref(subpatch.vert_start);
				closest.subd_quad_ref.set_ref(quad_ref_morton);
				closest.subd_quad_ref.set_hit_side(hit_side);

#endif
		}
	}
}
