#include "rni.h"

namespace wf {
	namespace gl {

		void batch_cam_ray_setup::run() {
			compute_shader cs("batch_cam_ray_setup",
							  	R"(
								#version 450
								layout (local_size_x = 32, local_size_y = 32) in;
							  	layout (std430, binding = 0) buffer rays_o  { vec4 rays_o  []; };
							  	layout (std430, binding = 1) buffer rays_d  { vec4 rays_d  []; };
							  	layout (std430, binding = 2) buffer rays_id { vec4 rays_id []; };
							  	layout (std430, binding = 3) buffer intersections { vec4 intersections[]; };
							  	layout (std430, binding = 4) buffer vertex_pos  { vec4 vertex_pos []; };
							  	layout (std430, binding = 5) buffer vertex_norm { vec4 vertex_norm[]; };
							  	layout (std430, binding = 6) buffer vertex_tc   { vec4 vertex_tc  []; };
							  	void main() {
							  	}
							  )");
			cs.compile();
			cs.bind();
			cs.dispatch(300, 400);
			cs.unbind();
		}
		
		void store_hitpoint_albedo::run() {
		}
	
	}
}

