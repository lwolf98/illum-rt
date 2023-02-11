#include "platform.h"
//#inlcude "bounce.h"

/* 
 * This file contains wf steps for the manylight algorithm
 *
 */

namespace wf::cpu {

	struct manylight_step : public wf::wire::manylight_step<raydata, per_sample_data<float>> {
		void run() override;
	};

	struct sample_v_0s : public wf::wire::sample_v_0s<raydata, vec3> {
		void run() override;
	};

	struct create_vpls : public wf::wire::create_vpls<raydata, vec3, vpl> {
		void run() override;
	};

	struct copy_vpls : public wf::wire::copy_vpls<vpl, std::vector<vpl>> {
		void run() override;
	};

	struct sample_vpls : public wf::wire::sample_vpls<raydata, std::vector<vpl>, vpl> {
		void run() override;
	};

	struct integrate_vpl_samples : public wf::wire::integrate_vpl_samples<raydata, vpl> {
		void run() override;
	};
	
}