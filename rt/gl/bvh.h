#pragma once

#include "base.h"
#include "shader.h"

#include "libgi/scene.h"

namespace wf {
	namespace gl {

		/*! \brief Default OpenGL BVH-based Ray Tracer
		 *
		 * 	Builds the BVH on the host using \ref binary_bvh_tracer and uploads it to the GPU
		 */
		class bbvh_rt : public batch_rt {
			compute_shader cs_closest, cs_any;
		public:
			bbvh_rt();
			void build(::scene *scene) override;
			void compute_closest_hit() override;
			void compute_any_hit() override;
		};

	}
}

