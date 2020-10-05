#include "seq.h"

#include <iostream>

using namespace std;

triangle_intersection seq_tri_is::closest_hit(const ray &ray) {
	triangle_intersection closest, intersection;
	for (int i = 0; i < scene->triangles.size(); ++i)
		if (intersect(scene->triangles[i], scene->vertices.data(), ray, intersection))
			if (intersection.t < closest.t) {
				closest = intersection;
				closest.ref = i;
			}
	return closest;
}

bool seq_tri_is::any_hit(const ray &ray) {
	triangle_intersection closest, intersection;
	for (int i = 0; i < scene->triangles.size(); ++i)
		if (intersect(scene->triangles[i], scene->vertices.data(), ray, intersection))
			return true;
	return false;
}


