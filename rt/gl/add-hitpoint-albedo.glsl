define(VERSION, 460)
define(HAVE_TEX, 1)
include(preamble.glsl)

void run(uint x, uint y) {
	uint id = y * w + x;
	vec4 hit = intersections[id];
	vec4 result = vec4(0);
	if (valid_hit(hit)) {
		ivec4 tri = triangles[hit_ref(hit)];
		material m = materials[tri.w];
		if (m.has_tex == 1) {
			vec2 tc = (1.0f - hit_beta(hit) - hit_gamma(hit)) * vertices[tri.x].tc
					  + hit_beta(hit) * vertices[tri.y].tc
					  + hit_gamma(hit) * vertices[tri.z].tc;
			// result = vec4(tc, 0, 1);
			result = texture(m.albedo_tex, tc);
		}
		else
			result = m.albedo;
	}
	result.w = 1; // be safe
	framebuffer[id] = result;
}
