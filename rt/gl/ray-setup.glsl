include(preamble.glsl)

uniform vec3 p, d, U, V;
uniform vec2 near_wh;
void run(uint x, uint y) {
	uint id = y * w + x;
	vec2 offset = vec2(0,0);
	float u = (float(x)+0.5+offset.x)/float(w) * 2.0f - 1.0f;	// \in (-1,1)
	float v = (float(y)+0.5+offset.y)/float(h) * 2.0f - 1.0f;
	u = near_wh.x * u;	// \in (-near_w,near_w)
	v = near_wh.y * v;
	vec3 dir = normalize(d + U*u + V*v);
	rays[id] = vec4(p, 1);
	rays[w*h+id] = vec4(dir, 0);
	rays[2*w*h+id] = vec4(vec3(1)/dir, 1);
}
