#include "bvh.h"

#include "libgi/timer.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>

#define K_T 1
#define K_I 1
#define error(x) { std::cerr << "command " << " (" << command << "): " << x << std::endl; return true;}
#define check_in_complete(x) { if (in.bad() || in.fail() || !in.eof()) error(x); }
using namespace glm;

// 
//    a more realistic binary bvh
//

template<bbvh_triangle_layout tr_layout> binary_bvh_tracer<tr_layout>::binary_bvh_tracer() {
}

template<bbvh_triangle_layout tr_layout> void binary_bvh_tracer<tr_layout>::build(::scene *scene) {
	time_this_block(build_bvh);
	this->scene = scene;
	std::cout << "Building BVH..." << std::endl;
	auto t1 = std::chrono::high_resolution_clock::now();

	// convert triangles to boxes
	std::vector<prim> prims(scene->triangles.size());
	std::vector<uint32_t> index(prims.size());
	for (int i = 0; i < prims.size(); ++i) {
		prims[i].grow(scene->vertices[scene->triangles[i].a].pos);
		prims[i].grow(scene->vertices[scene->triangles[i].b].pos);
		prims[i].grow(scene->vertices[scene->triangles[i].c].pos);
		index[i] = i;
	}

	if (binary_split_type == om) {
		root = subdivide_om(prims, index, 0, scene->triangles.size());
	}
	else if (binary_split_type == sm) {
		root = subdivide_sm(prims, index, 0, scene->triangles.size());
	}
// 	else if(binary_split_type == sah) {
// 		root = subdivide_sah(prims, index, 0, scene->triangles.size());
// 	}

	commit_shuffled_triangles(prims, index);

	auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	std::cout << "Done after " << duration << "ms" << std::endl;
}
	
template<bbvh_triangle_layout tr_layout>
template<bbvh_triangle_layout LO> 
typename std::enable_if<LO==bbvh_triangle_layout::flat,void>::type binary_bvh_tracer<tr_layout>::commit_shuffled_triangles(std::vector<prim> &prims, std::vector<uint32_t> &index) {
	std::vector<triangle> new_tris(scene->triangles.size());
	for (int i = 0; i < new_tris.size(); ++i)
		new_tris[i] = scene->triangles[index[i]];
	if (!esc) {
		scene->triangles = std::move(new_tris);
	}
	else {
		throw std::logic_error("This branch is not implemented yet -- and will complicate matters by making ESC a template argument");
	}
}

template<bbvh_triangle_layout tr_layout>
template<bbvh_triangle_layout LO> 
typename std::enable_if<LO!=bbvh_triangle_layout::flat,void>::type binary_bvh_tracer<tr_layout>::commit_shuffled_triangles(std::vector<prim> &prims, std::vector<uint32_t> &index) {
	this->index = std::move(index);
	if (esc) {
	}
}

template<bbvh_triangle_layout tr_layout>
uint32_t binary_bvh_tracer<tr_layout>::subdivide_om(std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) {
#ifndef RTGI_A02
	assert(start < end);
	auto p = [&](uint32_t i) { return prims[index[i]]; };

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end-start <= max_triangles_per_node) {
		uint32_t id = nodes.size();
		nodes.emplace_back();
		nodes[id].tri_offset(start);
		nodes[id].tri_count(end - start);
		return id;
	}

	// Bestimmen der Bounding Box der (Teil-)Szene
	aabb box;
	for (int i = start; i < end; ++i)
		box.grow(p(i));

	// Sortieren nach der größten Achse
	vec3 extent = box.max - box.min;
	float largest = max(extent.x, max(extent.y, extent.z));
	if (largest == extent.x)
		std::sort(index.data()+start, index.data()+end,
				  [&](uint32_t a, uint32_t b) { return prims[a].center().x < prims[b].center().x; });
	else if (largest == extent.y)
		std::sort(index.data()+start, index.data()+end,
				  [&](uint32_t a, uint32_t b) { return prims[a].center().y < prims[b].center().y; });
	else 
		std::sort(index.data()+start, index.data()+end,
				  [&](uint32_t a, uint32_t b) { return prims[a].center().z < prims[b].center().z; });

	// In der Mitte zerteilen
	int mid = start + (end-start)/2;
	uint32_t id = nodes.size();
	nodes.emplace_back();
	uint32_t l = subdivide_om(prims, index, start, mid);
	uint32_t r = subdivide_om(prims, index, mid,   end);
	nodes[id].link_l = l;
	nodes[id].link_r = r;
	for (int i = start; i < mid; ++i) nodes[id].box_l.grow(p(i));
	for (int i = mid;   i < end; ++i) nodes[id].box_r.grow(p(i));
	return id;
#else
	// todo
	std::logic_error("Not implemented, yet");
	return 0;
#endif
}

template<bbvh_triangle_layout tr_layout> uint32_t binary_bvh_tracer<tr_layout>::subdivide_sm(std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) {
#ifndef RTGI_A02
	assert(start < end);
	auto p = [&](uint32_t i) { return prims[index[i]]; };

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end-start <= max_triangles_per_node) {
		uint32_t id = nodes.size();
		nodes.emplace_back();
		nodes[id].tri_offset(start);
		nodes[id].tri_count(end - start);
		return id;
	}

	// Bestimmen der Bounding Box der (Teil-)Szene
	aabb box;
	for (int i = start; i < end; ++i)
		box.grow(p(i));

	// Bestimme und halbiere die größte Achse, sortiere die Dreieck(Schwerpunkt entscheidet) auf die richtige Seite
	// Nutze Object Median wenn Spatial Median in leeren Knoten resultiert
	vec3 extent = box.max - box.min;
	float largest = max(extent.x, max(extent.y, extent.z));
	float spatial_median;
	int mid = start;
	uint32_t* current_left  = index.data() + start;
	uint32_t* current_right = index.data() + end-1;

	auto sort_sm = [&](auto component_selector) {
		float spatial_median = component_selector(box.min + (box.max - box.min)*0.5f);
		while (current_left < current_right) {
			while (component_selector(prims[*current_left].center()) <= spatial_median && current_left < current_right) {
				current_left++;
				mid++;
			}
			while (component_selector(prims[*current_right].center()) > spatial_median && current_left < current_right) {
				current_right--;
			}
			if (component_selector(prims[*current_left].center()) > component_selector(prims[*current_right].center()) && current_left < current_right)
				std::swap(*current_left, *current_right);
		}
		if (mid == start || mid == end-1)  {
			std::sort(index.data()+start, index.data()+end,
			          [&](uint32_t a, uint32_t b) { return component_selector(prims[a].center()) < component_selector(prims[b].center()); });
			mid = start + (end-start)/2;
		}
	};
	
	if (largest == extent.x)      sort_sm([](const vec3 &v) { return v.x; });
	else if (largest == extent.y) sort_sm([](const vec3 &v) { return v.y; });
	else                          sort_sm([](const vec3 &v) { return v.z; });

	uint32_t id = nodes.size();
	nodes.emplace_back();
	uint32_t l = subdivide_sm(prims, index, start, mid);
	uint32_t r = subdivide_sm(prims, index, mid,   end);
	nodes[id].link_l = l;
	nodes[id].link_r = r;
	for (int i = start; i < mid; ++i) nodes[id].box_l.grow(p(i));
	for (int i = mid;   i < end; ++i) nodes[id].box_r.grow(p(i));
	return id;
#else
	// todo (optional)
	std::logic_error("Not implemented, yet");
	return 0;
#endif
}

template<bbvh_triangle_layout tr_layout> uint32_t binary_bvh_tracer<tr_layout>::subdivide_sah(std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) {
	assert(start < end);
	auto p = [&](uint32_t i) { return prims[index[i]]; };

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end-start <= max_triangles_per_node) {
		uint32_t id = nodes.size();
		nodes.emplace_back();
		nodes[id].tri_offset(start);
		nodes[id].tri_count(end - start);
		return id;
	}

	// Hilfsfunktionen
	auto box_surface = [&](const aabb &box) {
		vec3 extent = box.max - box.min;
		return (2*(extent.x*extent.y+extent.x*extent.z+extent.y*extent.z));
	};
		
	// Bestimmen der Bounding Box der (Teil-)Szene
	aabb box;
	for (int i = start; i < end; ++i)
		box.grow(p(i));

	// Teile die Box mit plane, sortiere die Dreiecke(Schwerpunkt entscheidet) auf die richtige Seite
	// bestimme box links, box rechts mit den jeweiligen kosten
	// Nutze Object Median wenn SAH plane in leeren Knoten resultiert
	// speichere mid, box links und box rechts für minimale gesamt Kosten
	vec3 extent = box.max - box.min;
	float largest = max(extent.x, max(extent.y, extent.z));
	float sah_cost_left = FLT_MAX;
	float sah_cost_right = FLT_MAX;
	int mid = start;
	bool use_om = true;
	aabb box_l, box_r;
	
	auto split = [&](auto component_selector, float plane) {
		int current_mid = start;
		uint32_t* current_left  = index.data() + start;
		uint32_t* current_right = index.data() + end-1;
		aabb current_box_l, current_box_r;
		while (current_left < current_right) {
			while (component_selector(prims[*current_left].center()) <= plane && current_left < current_right) {
				current_left++;
				current_mid++;
			}
			while (component_selector(prims[*current_right].center()) > plane && current_left < current_right) {
				current_right--;
			}
			if(component_selector(prims[*current_left].center()) > component_selector(prims[*current_right].center()) && current_left < current_right) {
				std::swap(*current_left, *current_right);
			}
		}
		if(current_mid == start || current_mid == end-1) {
			if (!use_om)
				return;
			std::sort(index.data()+start, index.data()+end,
					  [&](uint32_t a, uint32_t b) { return component_selector(prims[a].center()) < component_selector(prims[b].center()); });
			current_mid = start + (end-start)/2;
			use_om = false;
		}
		for (int i = start; i < current_mid; ++i) current_box_l.grow(p(i));
		for (int i = current_mid;   i < end; ++i) current_box_r.grow(p(i));
		float sah_cost_current_left = (box_surface(current_box_l)/box_surface(box))*(current_mid-start);
		float sah_cost_current_right = (box_surface(current_box_r)/box_surface(box))*(end-current_mid);
		if (sah_cost_current_left + sah_cost_current_right < sah_cost_left + sah_cost_right) {
			box_l = current_box_l;
			box_r = current_box_r;
			sah_cost_left = sah_cost_current_left;
			sah_cost_right = sah_cost_current_right;
			mid = current_mid;
		}
	};
	//Teile aktuelle Box mit NR_OF_PLANES gleichverteilten Ebenen
	//Anzahl der Ebenen=Anzahl der Dreiecke, wenn die Anzahl der Dreiecke kleiner als die Anzahl der Ebenen ist
	int current_number_of_planes = number_of_planes;
	if (end-start < current_number_of_planes) {
		current_number_of_planes = end-start;
	}
	if (largest == extent.x) {
		for (int i = 0; i < current_number_of_planes; ++i) {
			split([](const vec3 &v) { return v.x; }, box.min.x + (i+1)*(extent.x/(current_number_of_planes+1)));
		}
	}
	else if(largest == extent.y) {
		for (int i = 0; i < current_number_of_planes; ++i) {
			split([](const vec3 &v) { return v.y; }, box.min.y + (i+1)*(extent.y/(current_number_of_planes+1)));
		}
	}
	else {
		for (int i = 0; i < current_number_of_planes; ++i) {
			split([](const vec3 &v) { return v.z; }, box.min.z + (i+1)*(extent.z/(current_number_of_planes+1)));
		}
	}
	if (max_triangles_per_node > 1) {
		if ((K_I*(end - start)) < (K_T + K_I*(sah_cost_left + sah_cost_right)) && (end - start) <= max_triangles_per_node) {
			uint32_t id = nodes.size();
			nodes.emplace_back();
			nodes[id].tri_offset(start);
			nodes[id].tri_count(end - start);
			return id;
		}
	}
	uint32_t id = nodes.size();
	nodes.emplace_back();
	uint32_t l = subdivide_sah(prims, index, start, mid);
	uint32_t r = subdivide_sah(prims, index, mid,   end);
	nodes[id].link_l = l;
	nodes[id].link_r = r;
	nodes[id].box_l = box_l;
	nodes[id].box_r = box_r;
	return id;
}

template<bbvh_triangle_layout tr_layout> triangle_intersection binary_bvh_tracer<tr_layout>::closest_hit(const ray &ray) {
	time_this_block(closest_hit);
#ifndef RTGI_A02
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
				int tri_idx = triangle_index(node.tri_offset()+i);
				if (intersect(scene->triangles[tri_idx], scene->vertices.data(), ray, intersection))
					if (intersection.t < closest.t) {
						closest = intersection;
						closest.ref = tri_idx;
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

template<bbvh_triangle_layout tr_layout> bool binary_bvh_tracer<tr_layout>::any_hit(const ray &ray) {
	time_this_block(any_hit);
#ifndef RTGI_A02
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
				if (intersect(scene->triangles[node.tri_offset()+i], scene->vertices.data(), ray, intersection))
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

template<bbvh_triangle_layout tr_layout> bool binary_bvh_tracer<tr_layout>::interprete(const std::string &command, std::istringstream &in) {
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
			max_depth = depth_value;
			remove(filename.c_str());
			uint32_t nr = 0;
			export_bvh(root, &nr, 0, &filename);
			std::cout << "bvh exported to " << filename << std::endl;
			return true;
		}
		else error("Unknown bvh subcommand " << value);
	}
	return false;
}

template<bbvh_triangle_layout tr_layout> void binary_bvh_tracer<tr_layout>::export_bvh(uint32_t node_id, uint32_t *id, uint32_t depth, std::string *filename) {
	using namespace std;
	auto export_aabb = [&](const aabb box, const uint32_t vert[]) {
		ofstream out(*filename, ios::app);
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
	
	node current_node = nodes[node_id];
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
			export_bvh(current_node.link_l, id, depth+1, filename);
			export_bvh(current_node.link_r, id, depth+1, filename);
		}
	}
}

template<bbvh_triangle_layout tr_layout> void binary_bvh_tracer<tr_layout>::print_node_stats() {
	std::vector<int> leaf_nodes;
	uint32_t total_triangles = 0;
	uint32_t number_of_leafs = 0;
	int max = 0;
	int min = INT_MAX;
	int median = 0;
	for (typename std::vector<node>::iterator it = nodes.begin(); it != nodes.end(); ++it) {
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
template class binary_bvh_tracer<bbvh_triangle_layout::flat>;
template class binary_bvh_tracer<bbvh_triangle_layout::indexed>;

