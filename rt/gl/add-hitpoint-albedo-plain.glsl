include(preamble.glsl)

void run(uint x, uint y) {
	uint id = y * w + x;
	vec4 hit = intersections[id];
	vec4 result = vec4(0);
	if (valid_hit(hit)) {
		ivec4 tri = triangles[hit_ref(hit)];
		material m = materials[tri.w];
		result = m.albedo;
	}
	result.w = 1; // be safe
	framebuffer[id] = result;
}
