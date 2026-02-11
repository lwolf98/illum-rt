#include "rt.h"
#include "scene.h"
#include "libgi/subdivision.h"
#include <iostream>

using namespace glm;

diff_geom::diff_geom(const vertex &a, const vertex &b, const vertex &c,
					 const material *m, const triangle_intersection &is, const scene &scene)
 : x ((1.0f-is.beta-is.gamma)*a.pos  + is.beta*b.pos  + is.gamma*c.pos),
   ns(normalize((1.0f-is.beta-is.gamma)*a.norm + is.beta*b.norm + is.gamma*c.norm)),
   tc((1.0f-is.beta-is.gamma)*a.tc   + is.beta*b.tc   + is.gamma*c.tc),
   //ng(normalize(cross(b.pos-a.pos, c.pos-a.pos))),
   ng(normalize(cross(c.pos-a.pos, b.pos-a.pos))),
   tri(is.ref),
   mat(m) {
	dbg_v_a = a, dbg_v_b = b, dbg_v_c = c;
}

diff_geom::diff_geom(const triangle &t, const triangle_intersection &is, const scene &scene)
 : diff_geom(scene.vertices[t.a], scene.vertices[t.b], scene.vertices[t.c],
			 &scene.materials[t.material_id], is, scene) {
}

diff_geom::diff_geom(const triangle_intersection &is, const scene &scene)
 : diff_geom(scene.triangles[is.ref], is, scene) {
}

//[INDP_BOX]
/*#ifndef QUANTIZATION
void set_dbg_tri(diff_geom &dg, subd::subd_subpatch subpatch, uint32_t subd_quad_ref, bool upper) {
	vertex a, b, c;
	glm::mat3 M = inverse(subpatch.trafo);

	#ifndef SLAB_COMPRESSION
		const aabb &box = subpatch.box_from_index(subd_quad_ref);
	#else
		const aabb box = subpatch.box_from_index(subd_quad_ref);
	#endif

	#ifndef PROJECTION
		if (upper) {
			a.pos = M * vec3(box.min.x, box.max.y, box.min.z);
			b.pos = M * vec3(box.max.x, box.max.y, box.min.z);
			c.pos = M * vec3(box.min.x, box.max.y, box.max.z);
		}
		else {
			a.pos = M * vec3(box.max.x, box.max.y, box.max.z);
			b.pos = M * vec3(box.min.x, box.max.y, box.max.z);
			c.pos = M * vec3(box.max.x, box.max.y, box.min.z);
		}
	#else
		if (upper) {
			a.pos = vec3(box.min.x, box.min.y, box.min.z);
			b.pos = vec3(box.max.x, box.min.y, box.min.z);
			c.pos = vec3(box.min.x, box.min.y, box.max.z);
		}
		else {
			a.pos = vec3(box.max.x, box.min.y, box.max.z);
			b.pos = vec3(box.min.x, box.min.y, box.max.z);
			c.pos = vec3(box.max.x, box.min.y, box.min.z);
		}
		a.pos = M * subpatch.projected_to_oriented(a.pos);
		b.pos = M * subpatch.projected_to_oriented(b.pos);
		c.pos = M * subpatch.projected_to_oriented(c.pos);
	#endif

	dg.dbg_v_a.pos = a.pos;
	dg.dbg_v_b.pos = b.pos;
	dg.dbg_v_c.pos = c.pos;
}
#endif*/

diff_geom diff_geom::init(const triangle_intersection &is, const ray &is_ray, const scene &scene, bool debug) {
	if (is.ref < scene.triangles.size()) {
		return diff_geom(is, scene);
	}
	else {
		uint32_t patch_ref = ((uint32_t)-1) - is.ref;
		bool upper = is.subd_quad_ref.is_upper_tri();
		uint32_t subd_quad_ref = is.subd_quad_ref.ref();
		const subd::subd_patch &patch = scene.patches[patch_ref];
		const material &mat = scene.materials[patch.material_id];

#ifndef BOX_APPROXIMATION
		// exact geometry

		vertex a, b, c;
		triangle tri = patch.tri(subd_quad_ref, upper);
		a = patch.verts[tri.a];
		b = patch.verts[tri.b];
		c = patch.verts[tri.c];
	
		return diff_geom(a, b, c, &mat, is, scene);
#else
		// approximate geometry by bounding boxes

		uint32_t vert_quad_ref = patch.quad_ref_from_index(subd_quad_ref); //REVIEW: only temp until TC and normal data is stored in subpatches

		const subd::subd_subpatch &subpatch = patch.subpatch_from_index(subd_quad_ref);
		const glm::mat3 M = glm::inverse(subpatch.trafo);
		diff_geom dg(&mat, is.ref);
	//[INDP_BOX]
	//#ifndef QUANTIZATION
	//	set_dbg_tri(dg, subpatch, subd_quad_ref, upper);
	//#endif

		//REVIEW: put somewhere more suitable...
		auto [u, v] = patch.global_uvs(is.subd_quad_ref, is.beta, is.gamma);
		assert(u >= 0 && u <= 1);
		assert(v >= 0 && v <= 1);
		glm::vec2 u1 = patch.data[0].tc + (patch.data[1].tc - patch.data[0].tc) * u;
		glm::vec2 u2 = patch.data[2].tc + (patch.data[3].tc - patch.data[2].tc) * u;
		dg.tc = u1 + (u2 - u1) * v;

		dg.x = is_ray.o + is.t * is_ray.d;
		// TODO/REVIEW: adjust normal to the side of the box that has been hit
		switch (is.subd_quad_ref.hit_side()) {
			case BOX_SIDE_FRONT:      dg.ng = -M[1]; break;
			case BOX_SIDE_BACK:       dg.ng = M[1]; break;
			case BOX_SIDE_SIDE_LEFT:  dg.ng = cross(M[0], M[1]); break;
			case BOX_SIDE_SIDE_RIGHT: dg.ng = cross(M[1], M[0]); break;
			case BOX_SIDE_SIDE_DOWN:  dg.ng = cross(M[2], M[1]); break;
			case BOX_SIDE_SIDE_UP:    dg.ng = cross(M[1], M[2]); break;
			default:                  dg.ng = vec3(0);
		}

		if (def_intersect_box_mid)
			dg.x = dg.x + is.t_mid_box * -dg.ng;

	#ifndef SHADE_BY_GEOMETRY_NORMAL
		//TODO: interpolate normal, REVIW: correct like that??
		glm::vec3 n1 = patch.data[0].norm + (patch.data[1].norm - patch.data[0].norm) * u;
		glm::vec3 n2 = patch.data[2].norm + (patch.data[3].norm - patch.data[2].norm) * u;
		dg.ns = normalize(n1 + (n2 - n1) * v);
	#else
		dg.ns = dg.ng;
	#endif

		// TMP: Debugging
		/*vec3 dg_x = is_ray.o + is.t * is_ray.d;
		vec3 dg_ng = M[1];
		if (debug) {
			std::cout << "t: " << is.t << std::endl;
			std::cout << "x old: " << dg.x << ", ng old: " << dg.ng << std::endl;
			std::cout << "x new: " << dg_x << ", ng new: " << dg_ng << std::endl;
			std::cout << "mat    :" << subpatch.trafo[0] << ", " << subpatch.trafo[1] << ", " << subpatch.trafo[2] << std::endl;
			std::cout << "mat inv:" << M[0] << ", " << M[1] << ", " << M[2] << std::endl;
			std::cout << "ns: " << dg.ns << std::endl << std::endl;
		}
		dg.x = dg_x;
		dg.ng = dg_ng;*/

		return dg;
#endif
	}
}

vec3 diff_geom::albedo() const {
	if (mat->albedo_tex)
		return mat->albedo_tex->sample(tc);
	return mat->albedo;
}

vec3 diff_geom::emissive_albedo() const {
	vec3 color = albedo();
	if(color != vec3(0))	return color * mat->emissive;
	else					return mat->emissive;
}

float diff_geom::opacity() const {
	if (mat->albedo_tex)
		return mat->albedo_tex->sample(tc).w;
	return 1.0f;
};
