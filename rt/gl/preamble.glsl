ifdef(`VERSION', 
	`#version 'VERSION,
	`#version 450')

ifdef(`HAVE_TEX', `#extension GL_ARB_bindless_texture : require')
ifdef(`HAVE_TEX', `#extension GL_NV_gpu_shader5 : require') // AMD requires dynamically uniform access for arrays of samplers, which we cannot guarantee, so only allow this for NV

include(bindings.h)
ifdef(BLOCK_W, , `define(BLOCK_W, 32)')
ifdef(BLOCK_H, , `define(BLOCK_H, 32)')

// assumes 32 bit
#define FLT_MAX 3.402823466e+38
	
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
};

layout (local_size_x = BLOCK_W, local_size_y = BLOCK_H) in;
layout (std430, binding = BIND_RAYS) buffer b_rays        { vec4 rays  []; };
layout (std430, binding = BIND_ISEC) buffer b_intersections { vec4 intersections[]; };
layout (std430, binding = BIND_VERT) buffer b_vertex      { vertex vertices []; };
layout (std430, binding = BIND_TRIS) buffer b_triangles   { ivec4 triangles []; };
layout (std430, binding = BIND_NODE) buffer b_nodes       { bvh_node nodes []; };
layout (std430, binding = BIND_TIDS) buffer b_tri_ids     { uint tri_index []; };
layout (std430, binding = BIND_MTLS) buffer b_materials   { material materials []; };
layout (std430, binding = BIND_FBUF) buffer b_frambuffer  { vec4 framebuffer []; };
layout (std430, binding = BIND_TEXD) buffer b_texhack_data{ vec4 texhack_data []; };

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

