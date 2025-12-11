#include "rt.h"
#include "scene.h"
#include "libgi/subdivision.h"
//#include <iostream>

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

diff_geom diff_geom::init(const triangle_intersection &is, const scene &scene) {
	if (is.ref < scene.triangles.size()) {
		return diff_geom(is, scene);
	}
	else {
		uint32_t patch_ref = ((uint32_t)-1) - is.ref;
		bool upper = is.subd_quad_ref.is_upper_tri();
		uint32_t subd_quad_ref = is.subd_quad_ref.ref();
		const subd::subd_patch &patch = scene.patches[patch_ref];

		vertex a, b, c;
		//vertex dbg_a, dbg_b, dbg_c;

#ifdef BOX_APPROXIMATION
		// approximate geometry by bounding boxes

		uint32_t vert_quad_ref = patch.quad_ref_from_index(subd_quad_ref); //REVIEW: only temp until TC and normal data is stored in subpatches
		triangle tri = patch.tri(vert_quad_ref, upper);
		//a = patch.verts[tri.a];
		//b = patch.verts[tri.b];
		//c = patch.verts[tri.c];
		//dbg_a = a, dbg_b = b, dbg_c = c;

		const subd::subd_subpatch &subpatch = patch.subpatch_from_index(subd_quad_ref);
		const aabb &box = subpatch.box_from_index(subd_quad_ref);
		const glm::mat3 &M = glm::inverse(subpatch.trafo);

		//TODO/REVIEW: ! Check if min.y is always correct or if it also could be max.y !
		if (upper) {
			a.pos = M * vec3(box.min.x, box.min.y, box.min.z);
			b.pos = M * vec3(box.max.x, box.min.y, box.min.z);
			c.pos = M * vec3(box.min.x, box.min.y, box.max.z);
		}
		else {
			a.pos = M * vec3(box.max.x, box.min.y, box.max.z);
			b.pos = M * vec3(box.min.x, box.min.y, box.max.z);
			c.pos = M * vec3(box.max.x, box.min.y, box.min.z);
		}

#else
		// exact geometry

		triangle tri = patch.tri(subd_quad_ref, upper);
		a = patch.verts[tri.a];
		b = patch.verts[tri.b];
		c = patch.verts[tri.c];
		//dbg_a = a, dbg_b = b, dbg_c = c;
#endif
		const material &mat = scene.materials[patch.material_id];
		diff_geom dg(a, b, c, &mat, is, scene);
		// TODO: keep this assert?
		//if (mat.albedo_tex != 0) {
		//	assert(dg.tc.x >= 0 && dg.tc.x <= 1);
		//	assert(dg.tc.y >= 0 && dg.tc.y <= 1);
		//}

		/*{
			//DEBUG:
			triangle tri = patch.tri(subd_quad_ref, upper);
			dg.dbg_g_a = dbg_a;
			dg.dbg_g_b = dbg_b;
			dg.dbg_g_c = dbg_c;
		}*/

#ifdef BOX_APPROXIMATION
		//REVIEW: put somewhere more suitable...
		auto [u, v] = patch.global_uvs(is.subd_quad_ref, is.beta, is.gamma);
		assert(u >= 0 && u <= 1);
		assert(v >= 0 && v <= 1);
		glm::vec2 u1 = patch.data[0].tc + (patch.data[1].tc - patch.data[0].tc) * u;
		glm::vec2 u2 = patch.data[2].tc + (patch.data[3].tc - patch.data[2].tc) * u;
		dg.tc = u1 + (u2 - u1) * v;

		//TODO: interpolate normal, REVIW: correct like that??
		glm::vec3 n1 = patch.data[0].norm + (patch.data[1].norm - patch.data[0].norm) * u;
		glm::vec3 n2 = patch.data[2].norm + (patch.data[3].norm - patch.data[2].norm) * u;
		dg.ns = n1 + (n2 - n1) * v;

#ifdef PROJECTION
		dg.x = M * subpatch.projected_to_oriented(dg.x);
		dg.dbg_v_a.pos = M * subpatch.projected_to_oriented(dg.dbg_v_a.pos);
		dg.dbg_v_b.pos = M * subpatch.projected_to_oriented(dg.dbg_v_b.pos);
		dg.dbg_v_c.pos = M * subpatch.projected_to_oriented(dg.dbg_v_c.pos);
#endif
#endif

		return dg;
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
