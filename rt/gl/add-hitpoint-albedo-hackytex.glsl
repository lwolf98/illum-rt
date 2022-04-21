define(VERSION, 460)
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
			tc = fract(tc);
			vec2 dim = texhack_data[m.texhack_id].xy;
			int texel_y = int(tc.y * dim.y);
			int texel_x = int(tc.x * dim.x);
			if (texel_x < 0 || texel_x >= int(dim.x))
				result = vec4(1,0,0,1);
			else if (texel_y < 0 || texel_y >= int(dim.y))
				result = vec4(0,0,1,1);
			else {
				int texel = int(int(tc.y * dim.y) * dim.x + int(tc.x * dim.x));
				result = texhack_data[m.texhack_id + 1 + texel];
			}
		}
		else
			result = m.albedo;
	}
	result.w = 1; // be safe
	framebuffer[id] = result;
}
