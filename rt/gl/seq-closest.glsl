include(preamble.glsl)
include(tri-is.glsl)

// assumes 32 bit
#define FLT_MAX 3.402823466e+38
uniform int N;
void run(uint x, uint y) {
	uint id = y * w + x;
	vec4 closest = vec4(FLT_MAX, -1, -1, 0), is;
	vec4 o = rays[id],
	d = rays[w*h + id];
	for (int i = 0; i < N; ++i)
		if (intersect(i, o, d, vec2(0,FLT_MAX), is))
			if (is.x < closest.x) {
				closest = is;
				closest.w = intBitsToFloat(i);
			}
	intersections[id] = closest;
// 	ivec4 tri = triangles[0];
// 	vec4 a = vertices[tri.y].pos;
// 	intersections[id] = a;
}

