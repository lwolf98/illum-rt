#include "rt.h"
#include "scene.h"
#include "libgi/subdivision.h"

using namespace glm;

	
diff_geom::diff_geom(const vertex &a, const vertex &b, const vertex &c,
					 const material *m, const triangle_intersection &is, const scene &scene)
 : x ((1.0f-is.beta-is.gamma)*a.pos  + is.beta*b.pos  + is.gamma*c.pos),
   ns(normalize((1.0f-is.beta-is.gamma)*a.norm + is.beta*b.norm + is.gamma*c.norm)),
   tc((1.0f-is.beta-is.gamma)*a.tc   + is.beta*b.tc   + is.gamma*c.tc),
   ng(normalize(cross(b.pos-a.pos, c.pos-a.pos))),
   tri(is.ref),
   mat(m) {
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
		bool upper = is.subd_quad_ref > 0;
		uint32_t subd_quad_ref = abs(is.subd_quad_ref) - 1;
		const subd::subd_patch &patch = scene.patches[patch_ref];
		triangle tri = patch.tri(subd_quad_ref, upper);

		const vertex &a = patch.verts[tri.a];
		const vertex &b = patch.verts[tri.b];
		const vertex &c = patch.verts[tri.c];

		diff_geom dg(a, b, c, &scene.materials[patch.material_id], is, scene);
		assert(dg.tc.x >= 0 && dg.tc.x <= 1);
		assert(dg.tc.y >= 0 && dg.tc.y <= 1);
		return dg;
	}
}

vec3 diff_geom::albedo() const {
	if (mat->albedo_tex)
		return mat->albedo_tex->sample(tc);
	return mat->albedo;
}

float diff_geom::opacity() const {
	if (mat->albedo_tex)
		return mat->albedo_tex->sample(tc).w;
	return 1.0f;
};
