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
	
}