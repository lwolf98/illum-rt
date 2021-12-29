#pragma once

#include "base.h"
#include "shader.h"

#include "libgi/scene.h"

namespace wf {
	namespace gl {

		/*! \brief Overly naive sequential triangle intersector.
		 *
		 */
		struct seq_tri_is : public batch_rt {
			void build(::scene *scene) override;
			void compute_closest_hit() override;
			void compute_any_hit() override;
		};

	}
}

