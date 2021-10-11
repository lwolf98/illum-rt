#include "seq.h"

#include <iostream>

using namespace std;

/*
triangle_intersection seq_tri_is::closest_hit(const ray &ray) {
	triangle_intersection closest, intersection;
#ifndef RTGI_SKIP_SEQ_IS_IMPL
	for (int i = 0; i < scene->triangles.size(); ++i)
		if (intersect(scene->triangles[i], scene->vertices.data(), ray, intersection))
			if (intersection.t < closest.t) {
				closest = intersection;
				closest.ref = i;
			}
#endif
	return closest;
}

bool seq_tri_is::any_hit(const ray &ray) {
	triangle_intersection intersection;
#ifndef RTGI_SKIP_SEQ_IS_IMPL
	for (int i = 0; i < scene->triangles.size(); ++i)
		if (intersect(scene->triangles[i], scene->vertices.data(), ray, intersection))
			return true;
#endif
	return false;
}

*/

namespace wf {
	namespace gl {
		seq_tri_is::seq_tri_is()
		: vertex_pos("vertex_pos", 4, 0),
		  vertex_norm("vertex_norm", 5, 0),
		  vertex_tc("vertex_tc", 6, 0),
		  triangles("triangles", 7, 0) {
		}
		void seq_tri_is::build(::scene *scene) {
			triangles.resize(scene->triangles.size(), reinterpret_cast<ivec4*>(scene->triangles.data()));
			
			int N = scene->vertices.size();
			vector<vec4> v4(N);
			
			for (int i = 0; i < N; ++i)
				v4[i] = vec4(scene->vertices[i].pos, 1);
			vertex_pos.resize(v4);
			
			for (int i = 0; i < N; ++i)
				v4[i] = vec4(scene->vertices[i].norm, 0);
			vertex_norm.resize(v4);
			
			vector<vec2> v2(N);
			for (int i = 0; i < N; ++i)
				v2[i] = scene->vertices[i].tc;
			vertex_tc.resize(v2);
		}
		
		void seq_tri_is::compute_closest_hit() {
		}
		
		void seq_tri_is::compute_any_hit() {
		}
	}
}
