// Taken from PBRTv3 rng.h, see libgi/random.h
#define PCG32_MULT 0x5851f42d4c957f2dUL
#define FloatOneMinusEpsilon 0.99999994

float random_float(uint id) {
	uint64_t state = pcg_data[2*id+0];
	uint64_t inc   = pcg_data[2*id+1];
	uint64_t oldstate = state;
	state = oldstate * PCG32_MULT + inc;
	uint xorshifted = uint(((oldstate >> 18u) ^ oldstate) >> 27u);
	uint rot = uint(oldstate >> 59u);
	uint64_t tmp = (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
	float res = min(FloatOneMinusEpsilon, float(tmp) * 2.3283064365386963e-10f);
	pcg_data[2*id+0] = state;
	return res;
}

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


