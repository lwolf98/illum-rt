#pragma once

#include "base.h"
#include "shader.h"

#include "libgi/scene.h"

#include "rt/cpu/bvh.h"

namespace wf {
	namespace gl {

		/*! \brief Overly naive sequential triangle intersector.
		 *
		 */
		struct seq_tri_is : public batch_rt {
			void compute_closest_hit() override;
			void compute_any_hit() override;
		};
		
		struct glsl_bvh_node {
			vec4 box1min_l, box1max_r, box2min_o, box2max_c;
		};

		struct bvh : public batch_rt {
			ssbo<glsl_bvh_node> nodes;
			ssbo<uint32_t> indices;
			bvh();
			void build(::scene *scene) override;
			void compute_closest_hit() override;
			void compute_any_hit() override;
// 			void build(::scene *scene) override {}
// 			void compute_closest_hit() override {}
// 			void compute_any_hit() override {}
		};
	}
}

