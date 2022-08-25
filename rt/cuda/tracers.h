#pragma once

#include "base.h"
#include "libgi/scene.h"

namespace wf {
	namespace cuda {
		
		struct simple_rt : public batch_rt {
			simple_rt() : bvh_nodes("bvh_nodes", 0), bvh_index("index", 0) {};
			void build(scenedata *scene) override;
			void compute_hit(bool anyhit = false);

			global_memory_buffer<wf::cuda::simple_bvh_node> bvh_nodes;
			global_memory_buffer<uint32_t> bvh_index;
		};
		
		struct simple_rt_alpha : public batch_rt {
			simple_rt_alpha() : bvh_nodes("bvh_nodes", 0), bvh_index("index", 0) {};
			void build(scenedata *scene) override;
			void compute_hit(bool anyhit = false);

			global_memory_buffer<wf::cuda::simple_bvh_node> bvh_nodes;
			global_memory_buffer<uint32_t> bvh_index;
		};

		struct ifif                                  : public batch_rt { void compute_hit(bool anyhit = false); };
		struct ifif_alpha                            : public batch_rt { void compute_hit(bool anyhit = false); };
		struct whilewhile                            : public batch_rt { void compute_hit(bool anyhit) override; };
		struct whilewhile_alpha                      : public batch_rt { void compute_hit(bool anyhit) override; };
		struct dynamicwhilewhile                     : public batch_rt { void compute_hit(bool anyhit = false); };
		struct dynamicwhilewhile_alpha               : public batch_rt { void compute_hit(bool anyhit = false); };
		struct speculativewhilewhile                 : public batch_rt { void compute_hit(bool anyhit = false); };
		struct speculativewhilewhile_alpha           : public batch_rt { void compute_hit(bool anyhit = false); };
		struct persistentifif                        : public batch_rt { void compute_hit(bool anyhit = false); };
		struct persistentifif_alpha                  : public batch_rt { void compute_hit(bool anyhit = false); };
		struct persistentspeculativewhilewhile       : public batch_rt { void compute_hit(bool anyhit = false); };
		struct persistentspeculativewhilewhile_alpha : public batch_rt { void compute_hit(bool anyhit = false); };
		struct persistentwhilewhile                  : public batch_rt { void compute_hit(bool anyhit = false); };
		struct persistentwhilewhile_alpha            : public batch_rt { void compute_hit(bool anyhit = false); };

	}
}
