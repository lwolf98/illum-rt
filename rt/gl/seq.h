#pragma once

#include "base.h"
#include "shader.h"

#include "libgi/scene.h"

namespace wf {
	namespace gl {

		/*! \brief Overly naive sequential triangle intersector.
		 *
		 */
		class seq_tri_is : public batch_rt {
			compute_shader cs_closest, cs_any;
		public:
			seq_tri_is();
			void build(::scene *scene) override;
			void compute_closest_hit() override;
			void compute_any_hit() override;
		};

	}
}

