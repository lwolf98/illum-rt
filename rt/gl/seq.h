#pragma once

#include "base.h"

#include "libgi/scene.h"

namespace wf {
	namespace gl {
		class seq_tri_is : public batch_ray_tracer {
			ssbo<vec4> vertex_pos, vertex_norm;
			ssbo<vec2> vertex_tc;
			ssbo<ivec4> triangles;
		public:
			seq_tri_is();
			void build(::scene *scene) override;
			void compute_closest_hit() override;
			void compute_any_hit() override;
		};

	}
}

