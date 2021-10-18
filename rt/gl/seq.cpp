#include "seq.h"

#include <iostream>
#include <GL/glew.h>

using namespace std;

/*
triangle_intersection seq_tri_is::closest_hit(const ray &ray) {
	triangle_intersection closest, intersection;
#ifndef RTGI_SKIP_SEQ_IS_IMPL
	for (int i = 0; i < scene->triangles.size(); ++i)
		if (intersect(scene->triangles[i], scene->vertices.data(), ray, intersection))
			if (intersection.t < closest.t) {
				closest = intersection;
				closest.ref = i;
			}
#endif
	return closest;
}

bool seq_tri_is::any_hit(const ray &ray) {
	triangle_intersection intersection;
#ifndef RTGI_SKIP_SEQ_IS_IMPL
	for (int i = 0; i < scene->triangles.size(); ++i)
		if (intersect(scene->triangles[i], scene->vertices.data(), ray, intersection))
			return true;
#endif
	return false;
}

*/

namespace wf {
	namespace gl {
		const string tri_intersect = R"(
			bool intersect(vec4 tri_a, vec4 tri_b, vec4 tri_c, vec4 ray_o, vec4 ray_d, vec2 t_range, out vec4 info) {
				const float a_x = tri_a.x;
				const float a_y = tri_a.y;
				const float a_z = tri_a.z;

				const float a = a_x - tri_b.x;
				const float b = a_y - tri_b.y;
				const float c = a_z - tri_b.z;
				
				const float d = a_x - tri_c.x;
				const float e = a_y - tri_c.y;
				const float f = a_z - tri_c.z;
				
				const float g = ray_d.x;
				const float h = ray_d.y;
				const float i = ray_d.z;
				
				const float j = a_x - ray_o.x;
				const float k = a_y - ray_o.y;
				const float l = a_z - ray_o.z;

				float common1 = e*i - h*f;
				float common2 = g*f - d*i;
				float common3 = d*h - e*g;
				float M 	  = a * common1  +  b * common2  +  c * common3;
				float beta 	  = j * common1  +  k * common2  +  l * common3;

				common1       = a*k - j*b;
				common2       = j*c - a*l;
				common3       = b*l - k*c;
				float gamma   = i * common1  +  h * common2  +  g * common3;
				float tt    = -(f * common1  +  e * common2  +  d * common3);

				beta /= M;
				gamma /= M;
				tt /= M;	// opt: test before by *M

				if (tt > t_range.x && tt < t_range.y)
					if (beta > 0 && gamma > 0 && beta + gamma <= 1)
					{
						info.x = tt;
						info.y = beta;
						info.z = gamma;
						return true;
					}

				return false;
			}	
			bool intersect(int tri_id, vec4 ray_o, vec4 ray_d, vec2 t_range, out vec4 info) {
				ivec4 tri = triangles[tri_id];
				vec4 a = vertex_pos[tri.x];
				vec4 b = vertex_pos[tri.y];
				vec4 c = vertex_pos[tri.z];
				return intersect(a, b, c, ray_o, ray_d, t_range, info);
			}
		)";

		seq_tri_is::seq_tri_is()
		: cs_closest("seq_tri_is/closest",
					 platform::standard_preamble +
 					 tri_intersect + 
					 R"(
					 // assumes 32 bit
					 #define FLT_MAX 3.402823466e+38
					 uniform int N;
					 void run(uint x, uint y) {
			   			uint id = y * w + x;
					 	vec4 closest = vec4(FLT_MAX, -1, -1, 0), is;
						vec4 o = rays_o[id],
							 d = rays_d[id];
					 	for (int i = 0; i < N; ++i)
							if (intersect(i, o, d, vec2(0,FLT_MAX), is))
								if (is.x < closest.x) {
									closest = is;
									closest.w = intBitsToFloat(i);
								}
						intersections[id] = closest;
					 }
					 )"),
		  cs_any("seq_tri_is/any",
				 platform::standard_preamble +
 				 tri_intersect + 
				 R"(
				 // assumes 32 bit
				 #define FLT_MAX 3.402823466e+38
				 uniform int N;
				 void run(uint x, uint y) {
			   	    uint id = y * w + x;
				 	vec4 closest = vec4(FLT_MAX, -1, -1, 0), is;
				    vec4 o = rays_o[id],
				    	 d = rays_d[id];
				 	for (int i = 0; i < N; ++i)
				    	if (intersect(i, o, d, vec2(0,FLT_MAX), is))
							break;
				    intersections[id] = is;
				 }
				 )") {
			  cs_closest.compile();
		}

		void seq_tri_is::build(::scene *scene) {
			auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
			rt->sd.upload(scene);
		}
		
		void seq_tri_is::compute_closest_hit() {
			auto res = rc->resolution();
			cs_closest.bind();
			cs_closest.uniform("w", res.x).uniform("h", res.y);
			cs_closest.uniform("N", rc->scene.triangles.size());
			cs_closest.dispatch(res.x, res.y);
			cs_closest.unbind();
		}
		
		void seq_tri_is::compute_any_hit() {
			auto res = rc->resolution();
			cs_any.bind();
			cs_any.uniform("w", res.x).uniform("h", res.y);
			cs_any.uniform("N", rc->scene.triangles.size());
			cs_any.dispatch(res.x, res.y);
			cs_any.unbind();
		}
	}
}
