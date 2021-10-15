#include "rni.h"

#include <iostream>
using namespace std;

namespace wf {
	namespace gl {

		void batch_cam_ray_setup::run() {
			compute_shader cs("batch_cam_ray_setup",
							  	R"(
								#version 450
								layout (local_size_x = 32, local_size_y = 32) in;
							  	layout (std430, binding = 0) buffer b_rays_o  { vec4 rays_o  []; };
							  	layout (std430, binding = 1) buffer b_rays_d  { vec4 rays_d  []; };
							  	layout (std430, binding = 2) buffer b_rays_id { vec4 rays_id []; };
							  	layout (std430, binding = 3) buffer b_intersections { vec4 intersections[]; };
							  	layout (std430, binding = 4) buffer b_vertex_pos  { vec4 vertex_pos []; };
							  	layout (std430, binding = 5) buffer b_vertex_norm { vec4 vertex_norm[]; };
							  	layout (std430, binding = 6) buffer b_vertex_tc   { vec4 vertex_tc  []; };
								uniform int w;
								uniform int h;
							  	void main() {
									if (gl_GlobalInvocationID.x >= w || gl_GlobalInvocationID.y >= h)
										return;
									uint id = gl_GlobalInvocationID.y * w + gl_GlobalInvocationID.x;
									intersections[id].x = 123;
							  	}
							  )");
			auto res = rc->resolution();
			cs.compile();
			cs.bind();
			cs.uniform("w", res.x);
			cs.uniform("h", res.y);
			cs.dispatch(res.x, res.y);
			cs.unbind();
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

			// this conversion is going to be a pain
			auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
			rt->rd.intersections.download();
			triangle_intersection *is = (triangle_intersection*)rt->rd.intersections.org_data.data();
			cout << "---t---> " << is[0].t << endl;
		}
		
		void store_hitpoint_albedo::run() {
		}
	
	}
}

