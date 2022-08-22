include(preamble.glsl)

uniform layout(rgba32f,binding=1) image2D intersections;
uniform layout(rgba32f,binding=2) image2D framebuffer;

void run(uint x, uint y) {
	vec4 hit = imageLoad(intersections, ivec2(x, y));
	vec4 result = vec4(0);
	if (valid_hit(hit)) {
		ivec4 tri = triangles[hit_ref(hit)];
		material m = materials[tri.w];
		result = m.albedo;
	}
	result.w = 1; // be safe
	vec4 before = imageLoad(framebuffer, ivec2(x, y));
	imageStore(framebuffer, ivec2(x, y), before + result);
}
