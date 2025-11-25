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

		bool box_approximation = is.subd_quad_ref.level();
		vertex a, b, c;
		vertex dbg_a, dbg_b, dbg_c;
		if (box_approximation) {
			// approximate geometry by bounding boxes

			uint32_t vert_quad_ref = patch.quad_ref_from_index(subd_quad_ref); //REVIEW: only temp until TC and normal data is stored in subpatches
			triangle tri = patch.tri(vert_quad_ref, upper);
			a = patch.verts[tri.a];
			b = patch.verts[tri.b];
			c = patch.verts[tri.c];
			dbg_a = a, dbg_b = b, dbg_c = c;



			//if (patch_ref == 0 && subd_quad_ref == 3)
			//	std::cout << "";

			//assert(patch.subpatches.size() > 0);
			//uint32_t subpatch_size = patch.subpatches[0].len() - 1;
			//subpatch_size *= subpatch_size;

			//uint32_t aligned_subd_level = subpatches[0].subd_level;
			//uint32_t subpatch_id = subd_quad_ref >> 2*aligned_subd_level; // divide by subpatch size (#quads in subpatch)
			//const subd::subd_patch &subpatch = patch.subpatches[subpatch_id];
			const subd::subd_subpatch &subpatch = patch.subpatch_from_index(subd_quad_ref);

			//uint32_t modulo_mask = ~(0xFFFFFFFF << 2*aligned_subd_level);
			//uint32_t quad_ref_local = subd_quad_ref & modulo_mask;
			//uint32_t node_index = (quad_ref_local >> 2) + geom_series4(aligned_subd_level);
			//uint32_t box_index = quad_ref_local & 0x11;
			//const aabb &box = subpatch.nodes[node_index].boxes[box_index];
			const aabb &box = subpatch.box_from_index(subd_quad_ref);

			//std::cout << box.min << " --- " << box.max << std::endl;

			//const glm::mat3 &M = subpatch.trafo;
			const glm::mat3 &M = glm::inverse(subpatch.trafo);
			if (upper) {
				//c.pos = M * vec3(box.min.x, box.min.y, box.min.z);
				//a.pos = M * vec3(box.max.x, box.min.y, box.min.z);
				//b.pos = M * vec3(box.max.x, box.min.y, box.max.z);
				a.pos = M * vec3(box.min.x, box.max.y, box.min.z);
				b.pos = M * vec3(box.max.x, box.max.y, box.min.z);
				c.pos = M * vec3(box.min.x, box.max.y, box.max.z);
			}
			else {
				a.pos = M * vec3(box.max.x, box.max.y, box.max.z);
				b.pos = M * vec3(box.min.x, box.max.y, box.max.z);
				c.pos = M * vec3(box.max.x, box.max.y, box.min.z);
			}
		}
		else {
			// exact geometry

			triangle tri = patch.tri(subd_quad_ref, upper);
			//const vertex &a = patch.verts[tri.a];
			//const vertex &b = patch.verts[tri.b];
			//const vertex &c = patch.verts[tri.c];
			a = patch.verts[tri.a];
			b = patch.verts[tri.b];
			c = patch.verts[tri.c];
			dbg_a = a, dbg_b = b, dbg_c = c;
		}
		const material &mat = scene.materials[patch.material_id];
		diff_geom dg(a, b, c, &mat, is, scene);
		// TODO: keep this assert?
		//if (mat.albedo_tex != 0) {
		//	assert(dg.tc.x >= 0 && dg.tc.x <= 1);
		//	assert(dg.tc.y >= 0 && dg.tc.y <= 1);
		//}

		{
			//DEBUG:
			triangle tri = patch.tri(subd_quad_ref, upper);
			dg.dbg_g_a = dbg_a;
			dg.dbg_g_b = dbg_b;
			dg.dbg_g_c = dbg_c;
		}

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
