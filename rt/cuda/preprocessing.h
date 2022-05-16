#pragma once

#include "platform.h"
#include "base.h"

/* This file holds wavefront-steps that can be run when the scene is committed to the platform.
 * These steps could also make sense in a non-wavefront algorithm.
 *
 */

namespace wf::cuda {

	struct build_accel_struct : public wf::build_accel_struct {
		void run() override;
	};

	/*! This is mostly an examplary step that shows how scene data can be modified in place.
	 *  Here, we rotate the scene's geometry by successive rotations around X, Y and Z.
	 */
	struct rotate_scene : public wf::step {
		static constexpr char id[] = "rotate scene";
		void run() override;
	};

	/*! This is also an examplary step and shows how a different `view' of the original scene
	 *  can be constructed via the scene's and buffers' copy mechanisms.
	 *
	 *  Note that the thusly constructed scene view has to be dropped and pf->sd be reset to
	 *  the original scene when the view is no longer required.
	 */
	struct rotate_scene_keep_org : public wf::step {
		static constexpr char id[] = "rotate scene keeping original data";
		void run() override;
	};

	/*! This step can be used to remove a previously constructed scene view and set the previous
	 *  scene (view) back into effect.
	 */
	struct drop_scene_view : public wf::step {
		static constexpr char id[] = "drop scene view";
		void run() override;
	};
}
