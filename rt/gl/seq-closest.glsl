include(preamble.glsl)
include(tri-is.glsl)

uniform layout(rgba32f,binding=0) image2D rays;
uniform layout(rgba32f,binding=1) image2D is;

// assumes 32 bit
#define FLT_MAX 3.402823466e+38
uniform int N;
void run(uint x, uint y) {
	vec4 closest = vec4(FLT_MAX, -1, -1, 0), is;
	vec4 o = imageLoad(rays, ivec2(x, y));
	vec4 d = imageLoad(rays, ivec2(x, h+y));
	for (int i = 0; i < N; ++i)
		if (intersect(i, o, d, vec2(0,FLT_MAX), is))
			if (is.x < closest.x) {
				closest = is;
				closest.w = intBitsToFloat(i);
			}
	imageStore(is, ivec2(x, y), closest);
//	intersections[id] = closest;
// 	ivec4 tri = triangles[0];
// 	vec4 a = vertices[tri.y].pos;
// 	intersections[id] = a;
}

