ifdef(`VERSION', 
	`#version 'VERSION,
	`#version 450')

ifdef(`HAVE_TEX', `#extension GL_ARB_bindless_texture : require')
ifdef(`HAVE_TEX', `#extension GL_NV_gpu_shader5 : require') // AMD requires dynamically uniform access for arrays of samplers, which we cannot guarantee, so only allow this for NV

// for pcg random number generator
#extension GL_ARB_gpu_shader_int64 : require

include(bindings.h)
ifdef(BLOCK_W, , `define(BLOCK_W, 32)')
ifdef(BLOCK_H, , `define(BLOCK_H, 32)')

// assumes 32 bit
#define FLT_MAX 3.402823466e+38
	
#define M_PI 3.1415926535897932384626433832795
#define pi M_PI
#define one_over_pi (1.0/M_PI)
#define one_over_2pi (0.5/M_PI)
struct bvh_node {
	vec4 box1min_l, box1max_r, box2min_o, box2max_c;
};

struct vertex {
	vec4 pos, norm;
	vec2 tc;
	vec2 dummy;
};

struct material {
	vec4 albedo, emissive;
	ifdef(`HAVE_TEX',
		  `sampler2D albedo_tex;',
		  `uint texhack_id, dummy;') /* partitioning the uint64 this way is as unportable as code can get */
	int has_tex;
	float ior,roughness, dummy1, dummy2;
};

layout (local_size_x = BLOCK_W, local_size_y = BLOCK_H) in;
layout (std430, binding = BIND_VERT) buffer b_vertex      { vertex vertices []; };
layout (std430, binding = BIND_TRIS) buffer b_triangles   { ivec4 triangles []; };
layout (std430, binding = BIND_NODE) buffer b_nodes       { bvh_node nodes []; };
layout (std430, binding = BIND_TIDS) buffer b_tri_ids     { uint tri_index []; };
layout (std430, binding = BIND_MTLS) buffer b_materials   { material materials []; };
layout (std430, binding = BIND_TEXD) buffer b_texhack_data{ vec4 texhack_data []; };
layout (std430, binding = BIND_RRNG) buffer b_pcg_rng     { uint64_t pcg_data []; };

uniform int w;
uniform int h;

void run(uint x, uint y);

void main() {
	if (gl_GlobalInvocationID.x < w || gl_GlobalInvocationID.y < h)
		run(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
}

float hit_t(in vec4 hit) { return hit.x; }
float hit_beta(in vec4 hit) { return hit.y; }
float hit_gamma(in vec4 hit) { return hit.z; }
uint hit_ref(in vec4 hit) { return floatBitsToUint(hit.w); }
bool valid_hit(vec4 hit) {
	return hit.x != FLT_MAX;
}

vec3 bary_interpol(const vec4 hit, const vec3 a, const vec3 b, const vec3 c) {
	float alpha = 1.0 - hit_beta(hit) - hit_gamma(hit);
	return alpha * a + hit_beta(hit) * b + hit_gamma(hit) * c;
	
}

vec3 hit_ng(const vec4 hit, const ivec4 tri) {
	vec3 a = vertices[tri.x].norm.xyz;
	vec3 b = vertices[tri.y].norm.xyz;
	vec3 c = vertices[tri.z].norm.xyz;
	return bary_interpol(hit, a, b, c);
}

vec4 tex_lookup(const vec2 tc_in, const material m) {
	vec2 tc = fract(tc_in);
	vec4 result = vec4(1,0,1,1);
	vec2 dim = texhack_data[m.texhack_id].xy;
	int texel_y = int(tc.y * dim.y);
	int texel_x = int(tc.x * dim.x);
	if (texel_x < 0 || texel_x >= int(dim.x))
		result = vec4(1,0,1,1);
	else if (texel_y < 0 || texel_y >= int(dim.y))
		result = vec4(1,0,1,1);
	else {
		int texel = int(int(tc.y * dim.y) * dim.x + int(tc.x * dim.x));
		result = texhack_data[m.texhack_id + 1 + texel];
	}
	return result;
}
