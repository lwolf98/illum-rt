#include "platform.h"
//#inlcude "bounce.h"

/* 
 * This file contains wf steps for the manylight algorithm
 *
 */

namespace wf::cpu {

	struct sample_uniform_dir : public wf::wire::sample_uniform_dir<raydata, per_sample_data<float>> {
		void run() override;
	};
	
}