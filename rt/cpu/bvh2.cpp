#include "bvh.h"
#ifndef RTGI_SIMPLER_BBVH
#include "bvh-ctor.h"
#endif

#include "libgi/timer.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define error(x) { std::cerr << "command " << " (" << command << "): " << x << std::endl; return true;}
#define check_in_complete(x) { if (in.bad() || in.fail() || !in.eof()) error(x); }
using namespace glm;

#ifndef RTGI_SKIP_WF

struct scene_access_traits {
	scene *s;
	typedef ::triangle tri_t;
	int  triangles() { return s->triangles.size(); }
	tri_t triangle(int index) { return s->triangles[index]; }
	int triangle_a(int index) { return s->triangles[index].a; }
	int triangle_b(int index) { return s->triangles[index].b; }
	int triangle_c(int index) { return s->triangles[index].c; }
	vec3 vertex_pos(int index) { return s->vertices[index].pos; }
	void replace_triangles(std::vector<::triangle> &&new_tris) {
		s->triangles = new_tris;
	}
};

#endif

// 
//    a more realistic binary bvh
//

#ifndef RTGI_SIMPLER_BBVH

template<bbvh_triangle_layout tr_layout, bbvh_esc_mode esc_mode, bool alpha_aware>
binary_bvh_tracer<tr_layout, esc_mode, alpha_aware>::binary_bvh_tracer() {
}

template<bbvh_triangle_layout tr_layout, bbvh_esc_mode esc_mode, bool alpha_aware>
void binary_bvh_tracer<tr_layout, esc_mode, alpha_aware>::build(::scene *scene) {
	time_this_block(build_bvh);
	this->scene = scene;
	std::cout << "Building BVH..." << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();

	scene_access_traits st { scene };
	if (binary_split_type == om) {
		bvh_ctor_om<tr_layout, scene_access_traits> ctor(st, max_triangles_per_node);
		bvh = ctor.build(esc_mode == bbvh_esc_mode::on);
	}
	else if (binary_split_type == sm) {
		bvh_ctor_sm<tr_layout, scene_access_traits> ctor(st, max_triangles_per_node);
		bvh = ctor.build(esc_mode == bbvh_esc_mode::on);
	}
	else if(binary_split_type == sah) {
		bvh_ctor_sah<tr_layout, scene_access_traits> ctor(st, max_triangles_per_node, number_of_planes);
		bvh = ctor.build(esc_mode == bbvh_esc_mode::on);
	}

	auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	std::cout << "Done after " << duration << "ms" << std::endl;
}

template<bbvh_triangle_layout tr_layout, bbvh_esc_mode esc_mode, bool alpha_aware>
triangle_intersection binary_bvh_tracer<tr_layout, esc_mode, alpha_aware>::closest_hit(const ray &ray) {
	//time_this_block(closest_hit);
#ifndef RTGI_SKIP_BVH2_TRAV_IMPL
	triangle_intersection closest, intersection;
	uint32_t stack[25];
	int32_t sp = 0;
	stack[0] = bvh.root;
#ifdef COUNT_HITS
	unsigned int hits = 0;
#endif
	while (sp >= 0) {
		node node = bvh.nodes[stack[sp--]];
#ifdef COUNT_HITS
		hits++;
#endif
		if (node.inner()) {
			float dist_l, dist_r;
			bool hit_l = intersect4(node.box_l, ray, dist_l) && dist_l < closest.t;
			bool hit_r = intersect4(node.box_r, ray, dist_r) && dist_r < closest.t;
			if (hit_l && hit_r)
				if (dist_l < dist_r) {
					stack[++sp] = node.link_r;
					stack[++sp] = node.link_l;
				}
				else {
					stack[++sp] = node.link_l;
					stack[++sp] = node.link_r;
				}
			else if (hit_l)
				stack[++sp] = node.link_l;
			else if (hit_r)
				stack[++sp] = node.link_r;
		}
		else {
			for (int i = 0; i < node.tri_count(); ++i) {
				int tri_idx = triangle_index(node.tri_offset()+i);
				if (intersect(scene->triangles[tri_idx], scene->vertices.data(), ray, intersection)) {
					if constexpr(alpha_aware) {
						intersection.ref = tri_idx;
						diff_geom dg(intersection, *scene);
						if (dg.opacity() > ALPHA_THRESHOLD)
							if (intersection.t < closest.t) {
								closest = intersection;
								closest.ref = tri_idx;
							}
					} 
					else {
						if (intersection.t < closest.t) {
								closest = intersection;
								closest.ref = tri_idx;
							}
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
	std::logic_error("Not implemented, yet");
	return triangle_intersection();
#endif
}

template<bbvh_triangle_layout tr_layout, bbvh_esc_mode esc_mode, bool alpha_aware>
bool binary_bvh_tracer<tr_layout, esc_mode, alpha_aware>::any_hit(const ray &ray) {
	//time_this_block(any_hit);
#ifndef RTGI_SKIP_BVH2_TRAV_IMPL
	triangle_intersection intersection;
	uint32_t stack[25];
	int32_t sp = 0;
	stack[0] = bvh.root;
	while (sp >= 0) {
		node node = bvh.nodes[stack[sp--]];
		if (node.inner()) {
			float dist_l, dist_r;
			bool hit_l = intersect4(node.box_l, ray, dist_l);
			bool hit_r = intersect4(node.box_r, ray, dist_r);
			if (hit_l && hit_r)
				if (dist_l < dist_r) {
					stack[++sp] = node.link_r;
					stack[++sp] = node.link_l;
				}
				else {
					stack[++sp] = node.link_l;
					stack[++sp] = node.link_r;
				}
			else if (hit_l)
				stack[++sp] = node.link_l;
			else if (hit_r)
				stack[++sp] = node.link_r;
		}
		else {
			for (int i = 0; i < node.tri_count(); ++i) {
				int tri_idx = triangle_index(node.tri_offset()+i);
				if (intersect(scene->triangles[tri_idx], scene->vertices.data(), ray, intersection))
					if constexpr(alpha_aware) {
						intersection.ref = tri_idx;
						diff_geom dg(intersection, *scene);
						if (dg.opacity() > ALPHA_THRESHOLD)
							return true;
					}
					else {
						return true;
					}
			}
		}
	}
	return false;
#else
	// todo
	std::logic_error("Not implemented, yet");
	return false;
#endif
}

template<bbvh_triangle_layout tr_layout, bbvh_esc_mode esc_mode, bool alpha_aware>
bool binary_bvh_tracer<tr_layout, esc_mode, alpha_aware>::interprete(const std::string &command, std::istringstream &in) {
	std::string value;
	if (command == "bvh") {
		in >> value;
		if (value == "om") {
			binary_split_type = om;
			return true;
		}
		else if (value == "sm") {
			binary_split_type = sm;
			return true;
		}
		else if (value == "sah") {
			binary_split_type = sah;
			int temp;
			in >> temp;
			check_in_complete("Syntax error, \"bvh sah\" requires exactly one positive integral value");
			number_of_planes = temp;
			return true;
		}
		else if (value == "triangles") {
			in >> value;
			if (value == "multiple") {
				int temp;
				in >> temp;
				check_in_complete("Syntax error, \"triangles multiple\" requires exactly one positive integral value");
				max_triangles_per_node = temp;
			}
			else if (value == "single") max_triangles_per_node = 1;
			else error("Syntax error, \"bvh triangles\" requires a mode (single or multiple)");
			return true;
		}
		else if (value == "statistics") {
			print_node_stats();
			return true;
		}
		else if (value == "export") {
			int depth_value;
			std::string filename;
			in >> depth_value >> filename;
			check_in_complete("Syntax error, \"export\" requires exactly one positive integral value and a filename.obj");
			int max_depth = depth_value;
			remove(filename.c_str());
			uint32_t nr = 0;
			export_bvh(bvh.root, &nr, 0, filename, max_depth);
			std::cout << "bvh exported to " << filename << std::endl;
			return true;
		}
		else error("Unknown bvh subcommand " << value);
	}
	return false;
}

template<bbvh_triangle_layout tr_layout, bbvh_esc_mode esc_mode, bool alpha_aware>
void binary_bvh_tracer<tr_layout, esc_mode, alpha_aware>::export_bvh(uint32_t node_id, uint32_t *id, uint32_t depth, const std::string &filename, int max_depth) {
	using namespace std;
	auto export_aabb = [&](const aabb box, const uint32_t vert[]) {
		ofstream out(filename, ios::app);
		out << "v " << box.min.x << " " << box.min.y << " " << box.min.z << endl;
		out << "v " << box.max.x << " " << box.min.y << " " << box.min.z << endl;
		out << "v " << box.max.x << " " << box.max.y << " " << box.min.z << endl;
		out << "v " << box.min.x << " " << box.max.y << " " << box.min.z << endl;
		out << "v " << box.min.x << " " << box.min.y << " " << box.max.z << endl;
		out << "v " << box.max.x << " " << box.min.y << " " << box.max.z << endl;
		out << "v " << box.max.x << " " << box.max.y << " " << box.max.z << endl;
		out << "v " << box.min.x << " " << box.max.y << " " << box.max.z << endl;
		out << "g " << "level" << depth+1 << endl;
		out << "f " << vert[0] << " " << vert[1] << " " << vert[2] << " " << vert[3] << endl;
		out << "f " << vert[1] << " " << vert[5] << " " << vert[6] << " " << vert[2] << endl;
		out << "f " << vert[0] << " " << vert[1] << " " << vert[5] << " " << vert[4] << endl;
		out << "f " << vert[3] << " " << vert[2] << " " << vert[6] << " " << vert[7] << endl;
		out << "f " << vert[4] << " " << vert[7] << " " << vert[6] << " " << vert[5] << endl;
		out << "f " << vert[4] << " " << vert[0] << " " << vert[3] << " " << vert[7] << endl;
	};
	
	node current_node = bvh.nodes[node_id];
	if (current_node.inner()) {
		if(depth < max_depth) {
			uint32_t vertices[8];
			uint32_t current_id = (*id)++;
			for(int i=0; i<8; i++) {
				vertices[i] = current_id*8+i+1;
			}
			export_aabb(current_node.box_l, vertices);
			current_id = (*id)++;
			for(int i=0; i<8; i++) {
				vertices[i] = current_id*8+i+1;
			}
			export_aabb(current_node.box_r, vertices);
			export_bvh(current_node.link_l, id, depth+1, filename, max_depth);
			export_bvh(current_node.link_r, id, depth+1, filename, max_depth);
		}
	}
}

template<bbvh_triangle_layout tr_layout, bbvh_esc_mode esc_mode, bool alpha_aware>
void binary_bvh_tracer<tr_layout, esc_mode, alpha_aware>::print_node_stats() {
	std::vector<int> leaf_nodes;
	uint32_t total_triangles = 0;
	uint32_t number_of_leafs = 0;
	int max = 0;
	int min = INT_MAX;
	int median = 0;
	for (typename std::vector<node>::iterator it = bvh.nodes.begin(); it != bvh.nodes.end(); ++it) {
		if (!(it->inner())) {
			leaf_nodes.emplace_back(it->tri_count());
			if (it->tri_count() < min) min = it->tri_count();
			else if (it->tri_count() > max) max = it->tri_count();
			number_of_leafs++;
			total_triangles += it->tri_count();
		}
	}
	std::sort(leaf_nodes.begin(), leaf_nodes.end());
	if (number_of_leafs%2 == 1) {
		median = leaf_nodes.at(leaf_nodes.size()/2);
	}
	else {
		median = 0.5*(leaf_nodes.at(leaf_nodes.size()/2)+leaf_nodes.at(leaf_nodes.size()/2+1));
	}
	
	std::cout << "number of leaf nodes: " << number_of_leafs << std::endl;
	std::cout << "minimum triangles per node: " << min << std::endl;
	std::cout << "maximum triangles per node: " << max << std::endl;
	std::cout << "average triangles per node: " << (total_triangles/number_of_leafs) << std::endl;
	std::cout << "median of triangles per node: " << median << std::endl;
		
}

// trigger the two variants of interest to be generated
template class binary_bvh_tracer<bbvh_triangle_layout::flat,    bbvh_esc_mode::off>;
template class binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::off>;
template class binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>;
template class binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on, true>;

#else
	
binary_bvh_tracer::binary_bvh_tracer(binary_split_type type) : split_type(type) {
}

vec3 binary_bvh_tracer::center(const triangle &t) {
	return (scene->vertices[t.a].pos + scene->vertices[t.b].pos + scene->vertices[t.c].pos) / 3.0f;
}

void binary_bvh_tracer::build(::scene *scene) {
	time_this_block(build_bvh);
	this->scene = scene;
	std::cout << "Building BVH..." << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();
	
	aabb box;
	for (int i = 0; i < scene->triangles.size(); ++i) {
		box.grow(scene->vertices[scene->triangles[i].a].pos);
		box.grow(scene->vertices[scene->triangles[i].b].pos);
		box.grow(scene->vertices[scene->triangles[i].c].pos);
	}

	if (split_type == om)      root = subdivide_om(0,  scene->triangles.size(), box);
	else if (split_type == sm) root = subdivide_sm(0,  scene->triangles.size(), box);

	std::cout << nodes[0].box_l.min << " x " << nodes[0].box_l.max << std::endl;
	std::cout << nodes[0].box_r.min << " x " << nodes[0].box_r.max << std::endl;

	auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	std::cout << "Done after " << duration << "ms" << std::endl;
}
	
int32_t binary_bvh_tracer::subdivide_om(int start, int end, aabb box) {
#ifndef RTGI_SKIP_BVH2_OM_IMPL
	assert(start < end);

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end-start <= max_triangles_per_node) {
		uint32_t id = nodes.size();
		nodes.emplace_back();
		nodes[id].tri_offset(start);
		nodes[id].tri_count(end - start);
		return id;
	}

	// Sortieren nach der größten Achse
	vec3 extent = box.max - box.min;
	float largest = std::max(extent.x, std::max(extent.y, extent.z));
	if (largest == extent.x)
		std::sort(scene->triangles.data()+start, scene->triangles.data()+end,
				  [&](const triangle &a, const triangle &b) { return center(a).x < center(b).x; });
	else if (largest == extent.y)
		std::sort(scene->triangles.data()+start, scene->triangles.data()+end,
				  [&](const triangle &a, const triangle &b) { return center(a).y < center(b).y; });
	else 
		std::sort(scene->triangles.data()+start, scene->triangles.data()+end,
				  [&](const triangle &a, const triangle &b) { return center(a).z < center(b).z; });

	// Neue Node holen
	uint32_t id = nodes.size();
	nodes.emplace_back();
	// In der Mitte zerteilen
	int mid = start + (end-start)/2;
	for (int i = start; i < mid; ++i) {
		nodes[id].box_l.grow(scene->vertices[scene->triangles[i].a].pos);
		nodes[id].box_l.grow(scene->vertices[scene->triangles[i].b].pos);
		nodes[id].box_l.grow(scene->vertices[scene->triangles[i].c].pos);
	}
	for (int i = mid; i < end; ++i) {
		nodes[id].box_r.grow(scene->vertices[scene->triangles[i].a].pos);
		nodes[id].box_r.grow(scene->vertices[scene->triangles[i].b].pos);
		nodes[id].box_r.grow(scene->vertices[scene->triangles[i].c].pos);
	}
	// Achtung, nicht: node[id].link_l = subdivide(...)
	// Warum: zuerst wird "node[id].link_l" ausgewertet und referenziert speicher,
	//        danach wird subdivide ausgeführt und re-allokiert evtl den vektor,
	//        damit ist die zuvor schon referenzierte speicherstelle nicht mehr gültig!
	uint32_t l = subdivide_om(start, mid, nodes[id].box_l);
	uint32_t r = subdivide_om(mid,   end, nodes[id].box_r);
	nodes[id].link_l = l;
	nodes[id].link_r = r;
	return id;
#else
	// todo
	std::logic_error("Not implemented, yet");
	// Achtung, wenn die aktuelle Node den Index id hat, dann den rekursiven Aufruf nicht so machen:
	//   node[id].link_l = subdivide(...)
	// Warum: zuerst wird "node[id].link_l" ausgewertet und referenziert speicher,
	//        danach wird subdivide ausgeführt und re-allokiert evtl den vektor,
	//        damit ist die zuvor schon referenzierte speicherstelle nicht mehr gültig!
	return 0;
#endif
}

int32_t binary_bvh_tracer::subdivide_sm(int start, int end, aabb box) {
#ifndef RTGI_SKIP_BVH2_SM_IMPL
	assert(start < end);

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end-start <= max_triangles_per_node) {
		uint32_t id = nodes.size();
		nodes.emplace_back();
		nodes[id].tri_offset(start);
		nodes[id].tri_count(end - start);
		return id;
	}

	// Bestimme und halbiere die größte Achse, sortiere die Dreieck(Schwerpunkt entscheidet) auf die richtige Seite
	// Nutze Object Median wenn Spatial Median in leeren Knoten resultiert
	vec3 extent = box.max - box.min;
	float largest = std::max(extent.x, std::max(extent.y, extent.z));
	float spatial_median;
	int mid = start;
	uint32_t current_left  = start;
	uint32_t current_right = end-1;

	auto tri_ref = [&](int i)->triangle&{ return scene->triangles[i]; };

	auto sort_sm = [&](auto component_selector) {
		float spatial_median = component_selector(box.min + (box.max - box.min)*0.5f);
		while (current_left < current_right) {
			while (component_selector(center(tri_ref(current_left))) <= spatial_median && current_left < current_right) {
				current_left++;
				mid++;
			}
			while (component_selector(center(tri_ref(current_right))) > spatial_median && current_left < current_right) {
				current_right--;
			}
			if (component_selector(center(tri_ref(current_left))) > component_selector(center(tri_ref(current_right))) && current_left < current_right)
				std::swap(tri_ref(current_left), tri_ref(current_right));
		}
		if (mid == start || mid == end-1)  {
			std::sort(scene->triangles.data()+start, scene->triangles.data()+end,
			          [&](const triangle &a, const triangle &b) { return component_selector(center(a)) < component_selector(center(b)); });
			mid = start + (end-start)/2;
		}
	};
	
	if (largest == extent.x)      sort_sm([](const vec3 &v) { return v.x; });
	else if (largest == extent.y) sort_sm([](const vec3 &v) { return v.y; });
	else                          sort_sm([](const vec3 &v) { return v.z; });
	
	// Neue Node holen
	uint32_t id = nodes.size();
	nodes.emplace_back();
	// In der Mitte zerteilen
	for (int i = start; i < mid; ++i) {
		nodes[id].box_l.grow(scene->vertices[scene->triangles[i].a].pos);
		nodes[id].box_l.grow(scene->vertices[scene->triangles[i].b].pos);
		nodes[id].box_l.grow(scene->vertices[scene->triangles[i].c].pos);
	}
	for (int i = mid; i < end; ++i) {
		nodes[id].box_r.grow(scene->vertices[scene->triangles[i].a].pos);
		nodes[id].box_r.grow(scene->vertices[scene->triangles[i].b].pos);
		nodes[id].box_r.grow(scene->vertices[scene->triangles[i].c].pos);
	}
	uint32_t l = subdivide_sm(start, mid, nodes[id].box_l);
	uint32_t r = subdivide_sm(mid,   end, nodes[id].box_r);
	nodes[id].link_l = l;
	nodes[id].link_r = r;
	return id;
#else
	// todo
	std::logic_error("Not implemented, yet");
	return 0;
#endif
}

triangle_intersection binary_bvh_tracer::closest_hit(const ray &ray) {
#ifndef RTGI_SKIP_BVH2_TRAV_IMPL
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
			float dist_l, dist_r;
			bool hit_l = intersect4(node.box_l, ray, dist_l) && dist_l < closest.t;
			bool hit_r = intersect4(node.box_r, ray, dist_r) && dist_r < closest.t;
			if (hit_l && hit_r)
				if (dist_l < dist_r) {
					stack[++sp] = node.link_r;
					stack[++sp] = node.link_l;
				}
				else {
					stack[++sp] = node.link_l;
					stack[++sp] = node.link_r;
				}
			else if (hit_l)
				stack[++sp] = node.link_l;
			else if (hit_r)
				stack[++sp] = node.link_r;
		}
		else {
			for (int i = 0; i < node.tri_count(); ++i) {
				int tri_idx = node.tri_offset()+i;
				if (intersect(scene->triangles[tri_idx], scene->vertices.data(), ray, intersection)) {
					if (intersection.t < closest.t) {
						closest = intersection;
						closest.ref = tri_idx;
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
	std::logic_error("Not implemented, yet");
	return triangle_intersection();
#endif
}

bool binary_bvh_tracer::any_hit(const ray &ray) {
#ifndef RTGI_SKIP_BVH2_TRAV_IMPL
	triangle_intersection intersection;
	uint32_t stack[25];
	int32_t sp = 0;
	stack[0] = root;
	while (sp >= 0) {
		node node = nodes[stack[sp--]];
		if (node.inner()) {
			float dist_l, dist_r;
			bool hit_l = intersect4(node.box_l, ray, dist_l);
			bool hit_r = intersect4(node.box_r, ray, dist_r);
			if (hit_l && hit_r)
				if (dist_l < dist_r) {
					stack[++sp] = node.link_r;
					stack[++sp] = node.link_l;
				}
				else {
					stack[++sp] = node.link_l;
					stack[++sp] = node.link_r;
				}
			else if (hit_l)
				stack[++sp] = node.link_l;
			else if (hit_r)
				stack[++sp] = node.link_r;
		}
		else {
			for (int i = 0; i < node.tri_count(); ++i) {
				int tri_idx = node.tri_offset()+i;
				if (intersect(scene->triangles[tri_idx], scene->vertices.data(), ray, intersection))
					return true;
			}
		}
	}
	return false;
#else
	// todo
	std::logic_error("Not implemented, yet");
	return false;
#endif
}

bool binary_bvh_tracer::interprete(const std::string &command, std::istringstream &in) {
	return false;
}

#endif
