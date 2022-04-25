include(preamble.glsl)

uniform vec3 p, d, U, V;
uniform vec2 near_wh;

// Taken from PBRTv3 rng.h, see libgi/random.h
#define PCG32_MULT 0x5851f42d4c957f2dUL
#define FloatOneMinusEpsilon 0.99999994

vec2 random_float2(uint id) {
	uint64_t state = pcg_data[2*id+0];
	uint64_t inc   = pcg_data[2*id+1];
	vec2 res;
	uint64_t oldstate = state;
	state = oldstate * PCG32_MULT + inc;
	uint xorshifted = uint(((oldstate >> 18u) ^ oldstate) >> 27u);
	uint rot = uint(oldstate >> 59u);
	uint64_t tmp = (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
	res.x = min(FloatOneMinusEpsilon, float(tmp) * 2.3283064365386963e-10f);
	oldstate = state;
	state = oldstate * PCG32_MULT + inc;
	xorshifted = uint(((oldstate >> 18u) ^ oldstate) >> 27u);
	rot = uint(oldstate >> 59u);
	tmp = (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
	res.y = min(FloatOneMinusEpsilon, float(tmp) * 2.3283064365386963e-10f);
	pcg_data[2*id+0] = state;
	return res;
}

void run(uint x, uint y) {
	uint id = y * w + x;
	vec2 offset = random_float2(id);
	float u = (float(x)+offset.x)/float(w) * 2.0f - 1.0f;	// \in (-1,1)
	float v = (float(y)+offset.y)/float(h) * 2.0f - 1.0f;
	u = near_wh.x * u;	// \in (-near_w,near_w)
	v = near_wh.y * v;
	vec3 dir = normalize(d + U*u + V*v);
	rays[id] = vec4(p, 1);
	rays[w*h+id] = vec4(dir, 0);
	rays[2*w*h+id] = vec4(vec3(1)/dir, 1);
}
