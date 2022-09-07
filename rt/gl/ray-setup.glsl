include(preamble.glsl)

uniform vec3 p, d, U, V;
uniform vec2 near_wh;

uniform layout(rgba32f,binding=0) image2D rays;

include(random.glsl)

void run(uint x, uint y) {
	uint id = y * w + x;
	vec2 offset = random_float2(id);
	float u = (float(x)+offset.x)/float(w) * 2.0f - 1.0f;	// \in (-1,1)
	float v = (float(y)+offset.y)/float(h) * 2.0f - 1.0f;
	u = near_wh.x * u;	// \in (-near_w,near_w)
	v = near_wh.y * v;
	vec3 dir = normalize(d + U*u + V*v);
	imageStore(rays, ivec2(x, y), vec4(p, 1));
	imageStore(rays, ivec2(x, h+y), vec4(dir, 1));
	imageStore(rays, ivec2(x, 2*h+y), vec4(vec3(1)/dir, 1));
//	rays[id] = vec4(p, 1);
//	rays[w*h+id] = vec4(dir, 0);
//	rays[2*w*h+id] = vec4(vec3(1)/dir, 1);
}
