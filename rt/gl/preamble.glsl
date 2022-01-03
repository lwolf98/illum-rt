#version 450

ifdef(BLOCK_W, , `define(BLOCK_W, 32)')
ifdef(BLOCK_H, , `define(BLOCK_H, 32)')

	
struct bvh_node {
	vec4 box1min_l, box1max_r, box2min_o, box2max_c;
};


layout (local_size_x = BLOCK_W, local_size_y = BLOCK_H) in;
layout (std430, binding = 0) buffer b_rays_o  { vec4 rays_o  []; };
layout (std430, binding = 1) buffer b_rays_d  { vec4 rays_d  []; };
layout (std430, binding = 2) buffer b_rays_id { vec4 rays_id []; };
layout (std430, binding = 3) buffer b_intersections { vec4 intersections[]; };
layout (std430, binding = 4) buffer b_vertex_pos  { vec4 vertex_pos []; };
layout (std430, binding = 5) buffer b_vertex_norm { vec4 vertex_norm[]; };
layout (std430, binding = 6) buffer b_vertex_tc   { vec4 vertex_tc  []; };
layout (std430, binding = 7) buffer b_triangles   { ivec4 triangles []; };
layout (std430, binding = 8) buffer b_nodes       { bvh_node nodes []; };
layout (std430, binding = 9) buffer b_tri_ids     { uint tri_index []; };

uniform int w;
uniform int h;

void run(uint x, uint y);

void main() {
	if (gl_GlobalInvocationID.x < w || gl_GlobalInvocationID.y < h)
		run(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
}

