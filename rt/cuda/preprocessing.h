#pragma once

#include "platform.h"
#include "base.h"

namespace wf::cuda {

	struct build_accel_struct : public wf::build_accel_struct {
		void run() override;
	};

	struct rotate_scene : public wf::step {
		static constexpr char id[] = "rotate scene";
		void run() override;
	};

	struct rotate_scene_keep_org : public wf::step {
		static constexpr char id[] = "rotate scene keeping original data";
		void run() override;
	};

}
