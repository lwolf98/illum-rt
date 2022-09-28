#include "bvh.h"

#include "config.h"
#include <algorithm>

#ifdef HAVE_LIBEMBREE3
#include "rt/cpu/embree.h"
#endif

#ifdef ESC_DEBUG
#include <iostream>
#endif

template<bbvh_triangle_layout tr_layout, typename scene_traits>
struct bvh_ctor {
	template<bool cond, typename T>
    using variant = typename std::enable_if<cond, T>::type;
	scene_traits traits;

	typedef bbvh_node node;
	struct prim : public aabb {
		prim() : aabb() {}
		prim(const aabb &box, uint32_t tri_index) : aabb(box), tri_index(tri_index) {}
		vec3 center() const { return (min+max)*0.5f; }
		uint32_t tri_index;
	};
	
	void early_split_clipping(std::vector<prim> &prims, std::vector<uint32_t> &index);
	virtual uint32_t subdivide(::bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) = 0;
	
	// Finalize BVH by (potentiall) replacing the scene triangles and (in any case) making a flat list
	template<bbvh_triangle_layout LO=tr_layout>
	variant<LO==bbvh_triangle_layout::flat,void>
		commit_shuffled_triangles(bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index);

	template<bbvh_triangle_layout LO=tr_layout> 
	variant<LO!=bbvh_triangle_layout::flat,void>
		commit_shuffled_triangles(bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index);

	int max_triangles_per_node = 1;
	bool esc;

	bvh_ctor(scene_traits traits, int max_triangles_per_node) : max_triangles_per_node(max_triangles_per_node), traits(traits) {}
	bvh build(bool esc);
};

template<bbvh_triangle_layout tr_layout, typename scene_traits>
struct bvh_ctor_om : public bvh_ctor<tr_layout, scene_traits> { // final, remove esc mode param
	using base = bvh_ctor<tr_layout, scene_traits>;
	using prim = typename base::prim;
	using base::traits;
	using base::bvh_ctor;
	uint32_t subdivide(::bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) final override;
};

template<bbvh_triangle_layout tr_layout, typename scene_traits>
struct bvh_ctor_sm : public bvh_ctor<tr_layout, scene_traits> { // final, remove esc mode param
	using base = bvh_ctor<tr_layout, scene_traits>;
	using prim = typename base::prim;
	using base::traits;
	using base::bvh_ctor;
	uint32_t subdivide(::bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) final override;
};

template<bbvh_triangle_layout tr_layout, typename scene_traits>
struct bvh_ctor_sah : public bvh_ctor<tr_layout, scene_traits> { // final, remove esc mode param
	using base = bvh_ctor<tr_layout, scene_traits>;
	using prim = typename base::prim;
	using base::traits;
	int number_of_planes = 1;
	bvh_ctor_sah(scene_traits traits, int max_triangles_per_node, int number_of_planes) : bvh_ctor<tr_layout,scene_traits>(traits, max_triangles_per_node), number_of_planes(number_of_planes) {}
	uint32_t subdivide(::bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) final override;
};


#ifdef HAVE_LIBEMBREE3
template<bbvh_triangle_layout tr_layout, typename scene_traits>
struct bvh_ctor_embree : public bvh_ctor<tr_layout, scene_traits> {
	using base = bvh_ctor<tr_layout, scene_traits>;
	using prim = typename base::prim;
	using base::traits;
	using base::bvh_ctor;
	uint32_t subdivide(::bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) final override;
};
#endif


// 
// implementation
//


template<bbvh_triangle_layout tr_layout, typename scene_traits>
bvh bvh_ctor<tr_layout, scene_traits>::build(bool esc) {
	bvh res;
	// convert triangles to boxes
	std::vector<prim> prims(traits.triangles());
	std::vector<uint32_t> index(prims.size());
	for (int i = 0; i < prims.size(); ++i) {
		prims[i].grow(traits.vertex_pos(traits.triangle_a(i)));
		prims[i].grow(traits.vertex_pos(traits.triangle_b(i)));
		prims[i].grow(traits.vertex_pos(traits.triangle_c(i)));
		prims[i].tri_index = i; // with esc, this is different form the box index
		index[i] = i;
	}

// 	this->scene = scene;
	this->esc = esc;
	if (esc)
		early_split_clipping(prims, index);

	res.root = subdivide(res, prims, index, 0, prims.size());
	commit_shuffled_triangles(res, prims, index);
	return res;
}

static std::vector<aabb> split(std::vector<vec3> poly, float threshold) {
	auto area = [&](const aabb &box) {
		vec3 extent = box.max - box.min;
		return (2*(extent.x*extent.y+extent.x*extent.z+extent.y*extent.z));
	};

	aabb b;
	for (vec3 v : poly) b.grow(v);
	if (area(b) <= threshold)
		return {b};
	vec3 ext = b.max - b.min;
	float largest = std::max(ext.x, std::max(ext.y, ext.z));
	auto c = (largest == ext.x) ? [](const vec3 &v) { return v.x; }
	                            : (largest == ext.y) ? [](const vec3 &v) { return v.y; }
								                     : [](const vec3 &v) { return v.z; };
	float center = (c(b.max) + c(b.min)) * 0.5f;
	
	// find adjacent vertices that cross the split plane
	bool last_left = c(poly[0]) < center;
	int cross_first = -1, cross_second = -1;
	for (int i = 0; i < poly.size(); ++i) {
		bool left = c(poly[i]) < center;
		if (left != last_left)
			if (cross_first == -1)
				cross_first = i-1;
			else
				cross_second = i-1;
		last_left = left;
	}
	if (cross_second == -1) cross_second = poly.size()-1;
	assert(cross_first != -1);

	// compute intersection points on the split plane
	auto intersection_point = [&](int a) {
		float delta = center - c(poly[a]);
		int b = a+1 < poly.size() ? a+1 : 0;
		return poly[a] + (poly[b] - poly[a])  * (delta / (c(poly[b]) - c(poly[a])));
	};

	vec3 cross1 = intersection_point(cross_first);
	vec3 cross2 = intersection_point(cross_second);
	std::vector<vec3> poly1, poly2;
	for (int i = 0; i <= cross_first; ++i) poly1.push_back(poly[i]);
	poly1.push_back(cross1);
	poly2.push_back(cross1);
	for (int i = cross_first+1; i <= cross_second; ++i) poly2.push_back(poly[i]);
	poly2.push_back(cross2);
	poly1.push_back(cross2);
	for (int i = cross_second+1; i < poly.size(); ++i) poly1.push_back(poly[i]);

	poly1.erase(unique(poly1.begin(), poly1.end()), poly1.end());
	poly2.erase(unique(poly2.begin(), poly2.end()), poly2.end());

	auto sub1 = split(poly1, threshold);
	auto sub2 = split(poly2, threshold);
	std::vector<aabb> res;
	for (auto x : sub1) res.push_back(x);
	for (auto x : sub2) res.push_back(x);
	return res;
}

template<bbvh_triangle_layout tr_layout, typename scene_traits>
void bvh_ctor<tr_layout, scene_traits>::early_split_clipping(std::vector<prim> &prims, std::vector<uint32_t> &index) {
	std::vector<prim> stats = prims;
	auto area = [&](const prim &box) {
		vec3 extent = box.max - box.min;
		return (2*(extent.x*extent.y+extent.x*extent.z+extent.y*extent.z));
	};
	std::sort(std::begin(stats), std::end(stats), [&](const prim &a, const prim &b) { return area(a) < area(b); });
	float first = area(stats[0]);
	float last  = area(stats[stats.size()-1]);
	float q1 = area(stats[stats.size()/4]);
	float q2 = area(stats[stats.size()/2]);
	float q3 = area(stats[3*stats.size()/4]);
#ifdef ESC_DEBUG
	std::cout << first << "     [" << q1 << "   " << q2 << "   " << q3 << "]     " << last << std::endl;
#endif

	float q = area(stats[9*stats.size()/10]);
	float thres = q;
	
	auto pbox = [&](aabb b) {
#ifdef ESC_DEBUG
		std::cout << "[ " << glm::to_string(b.min) << "\t|  "<< glm::to_string(b.max) << " ]" << std::endl;
#endif
	};
	int N = prims.size(); // we modify the array as we go, but are only interested in the original elements
	for (int i = 0; i < N; ++i) {
		std::vector<vec3> poly;

		poly.push_back(traits.vertex_pos(traits.triangle_a(index[i])));
		poly.push_back(traits.vertex_pos(traits.triangle_b(index[i])));
		poly.push_back(traits.vertex_pos(traits.triangle_c(index[i])));
		
		std::vector<aabb> generated = split(poly, thres);
// 		for (int j = 0; j < generated.size(); ++j)
// 			pbox(generated[j]);
		prims[i] = prim(generated[0], i);
		for (int j = 1; j < generated.size(); ++j) {
			prims.push_back(prim(generated[j], i));
			index.push_back(prims.size()-1); // they all refer to the same triangle
		}
	}
#ifdef ESC_DEBUG
	std::cout << "ESC " << N << " --> " << prims.size() << " primitives" << std::endl;
#endif
}

template<bbvh_triangle_layout tr_layout, typename scene_traits>
template<bbvh_triangle_layout LO> 
typename std::enable_if<LO==bbvh_triangle_layout::flat,void>::type bvh_ctor<tr_layout, scene_traits>::commit_shuffled_triangles(bvh &bvh,
																															std::vector<prim> &prims,
																															std::vector<uint32_t> &index) {
	std::vector<typename scene_traits::tri_t> new_tris(index.size());
	for (int i = 0; i < new_tris.size(); ++i)
		new_tris[i] = traits.triangle(index[i]);
	traits.replace_triangles(std::move(new_tris));
}

template<bbvh_triangle_layout tr_layout, typename scene_traits>
template<bbvh_triangle_layout LO> 
typename std::enable_if<LO!=bbvh_triangle_layout::flat,void>::type bvh_ctor<tr_layout, scene_traits>::commit_shuffled_triangles(bvh &bvh,
																															std::vector<prim> &prims,
																															std::vector<uint32_t> &index) {
	if (esc)
		for (int i = 0; i < index.size(); ++i)
			index[i] = prims[index[i]].tri_index;
	bvh.index = std::move(index);
}


template<bbvh_triangle_layout tr_layout, typename scene_traits>
uint32_t bvh_ctor_om<tr_layout, scene_traits>::subdivide(::bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) {
#ifndef RTGI_SKIP_BVH2_OM_IMPL
	assert(start < end);
	auto p = [&](uint32_t i) { return prims[index[i]]; };

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end-start <= this->max_triangles_per_node) {
		uint32_t id = bvh.nodes.size();
		bvh.nodes.emplace_back();
		bvh.nodes[id].tri_offset(start);
		bvh.nodes[id].tri_count(end - start);
		return id;
	}

	// Bestimmen der Bounding Box der (Teil-)Szene
	aabb box;
	for (int i = start; i < end; ++i)
		box.grow(p(i));

	// Sortieren nach der größten Achse
	vec3 extent = box.max - box.min;
	float largest = std::max(extent.x, std::max(extent.y, extent.z));
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
	uint32_t id = bvh.nodes.size();
	bvh.nodes.emplace_back();
	uint32_t l = subdivide(bvh, prims, index, start, mid);
	uint32_t r = subdivide(bvh, prims, index, mid,   end);
	bvh.nodes[id].link_l = l;
	bvh.nodes[id].link_r = r;
	for (int i = start; i < mid; ++i) bvh.nodes[id].box_l.grow(p(i));
	for (int i = mid;   i < end; ++i) bvh.nodes[id].box_r.grow(p(i));
	return id;
#else
	// todo
	std::logic_error("Not implemented, yet");
	return 0;
#endif
}

template<bbvh_triangle_layout tr_layout, typename scene_traits>
uint32_t bvh_ctor_sm<tr_layout, scene_traits>::subdivide(::bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) {
#ifndef RTGI_SKIP_BVH2_SM_IMPL
	assert(start < end);
	auto p = [&](uint32_t i) { return prims[index[i]]; };

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end-start <= this->max_triangles_per_node) {
		uint32_t id = bvh.nodes.size();
		bvh.nodes.emplace_back();
		bvh.nodes[id].tri_offset(start);
		bvh.nodes[id].tri_count(end - start);
		return id;
	}

	// Bestimmen der Bounding Box der (Teil-)Szene
	aabb box;
	for (int i = start; i < end; ++i)
		box.grow(p(i));

	// Bestimme und halbiere die größte Achse, sortiere die Dreieck(Schwerpunkt entscheidet) auf die richtige Seite
	// Nutze Object Median wenn Spatial Median in leeren Knoten resultiert
	vec3 extent = box.max - box.min;
	float largest = std::max(extent.x, std::max(extent.y, extent.z));
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

	uint32_t id = bvh.nodes.size();
	bvh.nodes.emplace_back();
	uint32_t l = subdivide(bvh, prims, index, start, mid);
	uint32_t r = subdivide(bvh, prims, index, mid,   end);
	bvh.nodes[id].link_l = l;
	bvh.nodes[id].link_r = r;
	for (int i = start; i < mid; ++i) bvh.nodes[id].box_l.grow(p(i));
	for (int i = mid;   i < end; ++i) bvh.nodes[id].box_r.grow(p(i));
	return id;
#else
	// todo (optional)
	std::logic_error("Not implemented, yet");
	return 0;
#endif
}

template<bbvh_triangle_layout tr_layout, typename scene_traits>
uint32_t bvh_ctor_sah<tr_layout, scene_traits>::subdivide(::bvh &bvh, std::vector<prim> &prims, std::vector<uint32_t> &index, uint32_t start, uint32_t end) {
#ifndef RTGI_SKIP_BVH2_SAH_IMPL
	assert(start < end);
	auto p = [&](uint32_t i) { return prims[index[i]]; };

	// Rekursionsabbruch: Nur noch ein Dreieck in der Liste
	if (end-start <= this->max_triangles_per_node) {
		uint32_t id = bvh.nodes.size();
		bvh.nodes.emplace_back();
		bvh.nodes[id].tri_offset(start);
		bvh.nodes[id].tri_count(end - start);
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
	float box_surf = box_surface(box);
	vec3 extent = box.max - box.min;
	float largest = std::max(extent.x, std::max(extent.y, extent.z));
	int mid = start;
	aabb box_l, box_r;

	// Degenerierte BB (Linien) können nicht sinnvoll für eine Flächen-Heuristik verwendet werden.
	if (box_surf == 0) {
		if (largest == extent.x)
			std::sort(index.data()+start, index.data()+end,
					  [&](uint32_t a, uint32_t b) { return prims[a].center().x < prims[b].center().x; });
		else if (largest == extent.y)
			std::sort(index.data()+start, index.data()+end,
					  [&](uint32_t a, uint32_t b) { return prims[a].center().y < prims[b].center().y; });
		else 
			std::sort(index.data()+start, index.data()+end,
					  [&](uint32_t a, uint32_t b) { return prims[a].center().z < prims[b].center().z; });
		mid = start + (end-start)/2;
		for (int i = start; i < mid; ++i) box_l.grow(p(i));
		for (int i = mid;   i < end; ++i) box_r.grow(p(i));
	}
	else {
		// Teile die Box mit plane, sortiere die Dreiecke(Schwerpunkt entscheidet) auf die richtige Seite
		// bestimme box links, box rechts mit den jeweiligen kosten
		// Nutze Object Median wenn SAH plane in leeren Knoten resultiert
		// speichere mid, box links und box rechts für minimale gesamt Kosten
		float sah_cost_left = FLT_MAX;
		float sah_cost_right = FLT_MAX;
		bool use_om = true;

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
				while (component_selector(prims[*current_right].center()) > plane && current_left < current_right)
					current_right--;
				if(component_selector(prims[*current_left].center()) > component_selector(prims[*current_right].center()) && current_left < current_right)
					std::swap(*current_left, *current_right);
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
			float sah_cost_current_left = (box_surface(current_box_l)/box_surf)*(current_mid-start);
			float sah_cost_current_right = (box_surface(current_box_r)/box_surf)*(end-current_mid);
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
		int current_number_of_planes = this->number_of_planes;
		if (end-start < current_number_of_planes)
			current_number_of_planes = end-start;

		if (largest == extent.x)
			for (int i = 0; i < current_number_of_planes; ++i)
				split([](const vec3 &v) { return v.x; }, box.min.x + (i+1)*(extent.x/(current_number_of_planes+1)));
		else if(largest == extent.y)
			for (int i = 0; i < current_number_of_planes; ++i)
				split([](const vec3 &v) { return v.y; }, box.min.y + (i+1)*(extent.y/(current_number_of_planes+1)));
		else
			for (int i = 0; i < current_number_of_planes; ++i)
				split([](const vec3 &v) { return v.z; }, box.min.z + (i+1)*(extent.z/(current_number_of_planes+1)));

		constexpr float K_T = 1;
		constexpr float K_I = 1;
		if (this->max_triangles_per_node > 1) {
			if ((K_I*(end - start)) < (K_T + K_I*(sah_cost_left + sah_cost_right)) && (end - start) <= this->max_triangles_per_node) {
				uint32_t id = bvh.nodes.size();
				bvh.nodes.emplace_back();
				bvh.nodes[id].tri_offset(start);
				bvh.nodes[id].tri_count(end - start);
				return id;
			}
		}
	}
	uint32_t id = bvh.nodes.size();
	bvh.nodes.emplace_back();
	uint32_t l = subdivide(bvh, prims, index, start, mid);
	uint32_t r = subdivide(bvh, prims, index, mid,   end);
	bvh.nodes[id].link_l = l;
	bvh.nodes[id].link_r = r;
	bvh.nodes[id].box_l = box_l;
	bvh.nodes[id].box_r = box_r;
	return id;
#else
	// todo (highly optional)
	std::logic_error("Not implemented, yet");
	return 0;
#endif
}

#ifdef HAVE_LIBEMBREE3
template<bbvh_triangle_layout tr_layout, typename scene_traits>
uint32_t bvh_ctor_embree<tr_layout, scene_traits>::subdivide(::bvh &bvh,
															 std::vector<prim> &prims,
															 std::vector<uint32_t> &index,
															 uint32_t start,
															 uint32_t end) {
	embree_tracer em;
	RTCBVH em_bvh = rtcNewBVH(em.em_device);

	//Setup the primitives and callback data
	std::vector<RTCBuildPrimitive> build_primitives;
	build_primitives.reserve(prims.size() * 2); //Extra space to allow best quality bvh creation
	build_primitives.resize(prims.size());
	bvh_callback_data cb_data;
	cb_data.bvh_nodes.reserve(build_primitives.size() * 2);
	pthread_rwlock_init(&cb_data.lock, 0);

	//Initialize the primitives
	for(int i = 0; i < prims.size(); i++)
	{
		build_primitives[i].geomID  = 0;
		build_primitives[i].primID  = prims[i].tri_index;
		build_primitives[i].lower_x = prims[i].min.x;
		build_primitives[i].lower_y = prims[i].min.y;
		build_primitives[i].lower_z = prims[i].min.z;
		build_primitives[i].upper_x = prims[i].max.x;
		build_primitives[i].upper_y = prims[i].max.y;
		build_primitives[i].upper_z = prims[i].max.z;
	}

	//Configuration
	RTCBuildArguments arguments = rtcDefaultBuildArguments();
	arguments.byteSize = sizeof(arguments);
	arguments.buildFlags = RTC_BUILD_FLAG_NONE;
	arguments.buildQuality = RTC_BUILD_QUALITY_HIGH;
	arguments.maxBranchingFactor = 2;
	arguments.maxDepth = 1024;
	arguments.sahBlockSize = 1;
	arguments.minLeafSize = 1;
	arguments.maxLeafSize = 1;
	arguments.traversalCost = 1.0f;
	arguments.intersectionCost = 1.0f;
	arguments.bvh = em_bvh;
	arguments.primitives = build_primitives.data();
	arguments.primitiveCount = build_primitives.size();
	arguments.primitiveArrayCapacity = build_primitives.capacity();
	arguments.createNode = embvh_create_node;
	arguments.setNodeChildren = embvh_set_node_children;
	arguments.setNodeBounds = embvh_set_node_bounds;
	arguments.createLeaf = embvh_create_leaf;
	arguments.splitPrimitive = embvh_split_primitive;
	arguments.buildProgress = nullptr;
	arguments.userPtr = static_cast<void*>(&cb_data);

	// Ugh, but I did not find a better solution to return the index over void*
	// Maybe better to first build the bvh as a "linked tree" and
	// then convert it to a vector afterwards so we can avoid this
	assert(sizeof(void*) == 8);
	bvh.root = reinterpret_cast<uint64_t>(rtcBuildBVH(&arguments));
	bvh.nodes = cb_data.bvh_nodes;
	rtcReleaseBVH(em_bvh);
	for(int i = 0; i < index.size(); i++)
		index[i] = i;
	return bvh.root;
}
#endif
