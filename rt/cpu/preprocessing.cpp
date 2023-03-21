#include "preprocessing.h"

namespace wf::cpu {
		
	void build_accel_struct::run() {
		pf->rt->build(pf->sd);
	}

	void compute_light_distribution::run() {
		// this uses pre-BVH triangles
		// using the 2nd below code that goes over the tris yields the SAME distribution BUT different results.
		// probably because we use the distribution computed right here, can we as a test feed this routine
		// the noe-found triangles?
		// how to agree on which data to use, the post-BVH triangles are shuffled properly, but emissive triangles might have been duplicated due to ESC
		//
		// or rather let's do this
		// - finding the emissive tris at scene load is find
		// - when the scene changes, all scene steps need to be re-evaluated
		// - light-distribution is an algo-specific scene step, not a regular algo step
		pf->sd->compute_light_distribution();
		
		/*
		auto &tris = pf->sd->triangles;
		auto &verts = pf->sd->vertices;
		std::vector<int> light_id;
		std::vector<float> light_power;
		for (int i = 0; i < tris.size(); ++i) {
			const material &m = rc->scene.materials[tris[i].material_id];
			if (m.emissive != vec3(0)) {
				auto t = tris[i];
				const vertex &a = verts[tris[i].a];
				const vertex &b = verts[tris[i].b];
				const vertex &c = verts[tris[i].c];
				vec3 e1 = vec3(b.pos) - vec3(a.pos);
				vec3 e2 = vec3(c.pos) - vec3(a.pos);
				
				light_id.push_back(i);
				light_power.push_back(luma(m.emissive) * 0.5f * length(cross(e1,e2)) * pi);
			}
		}
		*/

	}

}
