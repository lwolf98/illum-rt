include(preamble.glsl)
include(tri-is.glsl)

define(inner, ($1.box2max_c.w == 0))
define(link_l, int($1.box1min_l.w))
define(link_r, int($1.box1max_r.w))
define(tri_offset, int($1.box2min_o.w))
define(tri_count,  int($1.box2max_c.w))

uniform layout(rgba32f,binding=0) image2D rays;
uniform layout(rgba32f,binding=1) image2D intersections;

bool intersect4(vec4 box_min, vec4 box_max, const vec4 ray_o, const vec4 ray_id, vec2 range, out float is) {
	vec3 t1_tmp = ((box_min - ray_o) * ray_id).xyz;
	vec3 t2_tmp = ((box_max - ray_o) * ray_id).xyz;
	vec3 t1v = min(t1_tmp, t2_tmp);
	vec3 t2v = max(t1_tmp, t2_tmp);
	float t1 = max(t1v.x, max(t1v.y, t1v.z));
	float t2 = min(t2v.x, min(t2v.y, t2v.z));

	if (t1 > t2)      return false;
	if (t2 < range.x) return false;
	if (t1 > range.y) return false;
		
	is = t1;
	return true;
}


void run_simple(uint x, uint y) {
	vec4 hitpoint = vec4(FLT_MAX, -1, -1, 0), is;
	vec4 o = imageLoad(rays, ivec2(x, y));
	vec4 d = imageLoad(rays, ivec2(x, h+y));
	vec4 inv_d = imageLoad(rays, ivec2(x, 2*h+y));
	
	#define STACKSIZE 24
	uint stack[STACKSIZE];
	int sp = 0;
	stack[0] = 0;
	bvh_node node;
	while (sp >= 0) {
		node = nodes[stack[sp--]];
		if (inner(node)) {
			float dist_l, dist_r;
			bool hit_l = intersect4(node.box1min_l, node.box1max_r, o, inv_d, vec2(0,FLT_MAX), dist_l);
			bool hit_r = intersect4(node.box2min_o, node.box2max_c, o, inv_d, vec2(0,FLT_MAX), dist_r);
			if (hit_l && hit_r) {
				stack[++sp] = link_r(node);
				stack[++sp] = link_l(node);
			}
			else if (hit_l)
				stack[++sp] = link_l(node);
			else if (hit_r)
				stack[++sp] = link_r(node);
			if (sp > STACKSIZE) 
				break;
		}
		else {
			uint start = tri_offset(node);
			uint count = tri_count(node);
			for (uint i = 0; i < count; ++i) {
				uint tri_id = tri_index[start+i];
				vec4 is;
				if (intersect(int(tri_id), o, d, vec2(o.w,d.w), is))
					if (is.x < hitpoint.x) {
						hitpoint = is;
						hitpoint.w = uintBitsToFloat(tri_id);
					}
			}
			if (hitpoint.x != FLT_MAX)
				break;
		}
	}
	imageStore(intersections, ivec2(x, y), hitpoint);
}

void run_reduce_stackuse(uint x, uint y) {
	vec4 hitpoint = vec4(FLT_MAX, -1, -1, 0), is;
	vec4 o = imageLoad(rays, ivec2(x, y));
	vec4 d = imageLoad(rays, ivec2(x, h+y));
	vec4 inv_d = imageLoad(rays, ivec2(x, 2*h+y));

	#define STACKSIZE 24
	uint stack[STACKSIZE];
	int sp = -1;
	stack[0];
	bvh_node node = nodes[0];
	while (true) {
		if (inner(node)) {
			float dist_l, dist_r;
			bool hit_l = intersect4(node.box1min_l, node.box1max_r, o, inv_d, vec2(0,FLT_MAX), dist_l);
			bool hit_r = intersect4(node.box2min_o, node.box2max_c, o, inv_d, vec2(0,FLT_MAX), dist_r);
			if (hit_l && hit_r) {
					stack[++sp] = link_r(node);
					node = nodes[link_l(node)];
			}
			else if (hit_l)
				node = nodes[link_l(node)];
			else if (hit_r)
				node = nodes[link_r(node)];
			else if (sp >= 0)
				node = nodes[stack[sp--]];
			else
				break;
			if (sp > STACKSIZE) 
				break;
		}
		else {
			uint start = tri_offset(node);
			uint count = tri_count(node);
			for (uint i = 0; i < count; ++i) {
				uint tri_id = tri_index[start+i];
				vec4 is;
				if (intersect(int(tri_id), o, d, vec2(o.w,d.w), is))
					if (is.x < hitpoint.x) {
						hitpoint = is;
						hitpoint.w = uintBitsToFloat(tri_id);
					}
			}
			if (hitpoint.x != FLT_MAX)
				break;
			if (sp >= 0)
				node = nodes[stack[sp--]];
			else
				break;
		}
	}
	imageStore(intersections, ivec2(x, y), hitpoint);
}

void run(uint x, uint y) {
 	run_simple(x, y);
//	run_reduce_stackuse(x, y);
}
