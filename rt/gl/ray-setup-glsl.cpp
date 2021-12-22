#include "shader.h"
namespace wf::gl {
	compute_shader ray_setup_shader("ray_setup", R"(



#version 450
layout (local_size_x = 32, local_size_y = 32) in;
layout (std430, binding = 0) buffer b_rays_o  { vec4 rays_o  []; };
layout (std430, binding = 1) buffer b_rays_d  { vec4 rays_d  []; };
layout (std430, binding = 2) buffer b_rays_id { vec4 rays_id []; };
layout (std430, binding = 3) buffer b_intersections { vec4 intersections[]; };
layout (std430, binding = 4) buffer b_vertex_pos  { vec4 vertex_pos []; };
layout (std430, binding = 5) buffer b_vertex_norm { vec4 vertex_norm[]; };
layout (std430, binding = 6) buffer b_vertex_tc   { vec4 vertex_tc  []; };
layout (std430, binding = 7) buffer b_triangles   { ivec4 triangles []; };

uniform int w;
uniform int h;

void run(uint x, uint y);

void main() {
	if (gl_GlobalInvocationID.x < w || gl_GlobalInvocationID.y < h)
		run(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
}



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
	rays_o[id] = vec4(p, 1);
	rays_d[id] = vec4(dir, 0);
	rays_id[id] = vec4(vec3(1)/dir, 1);
}
	)");
}
