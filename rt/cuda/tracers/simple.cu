#include "simple.h"

#include "libgi/timer.h"

#include <iostream>

#include "kernels.h"
#include "cuda-helpers.h"

namespace wf{
	namespace cuda{
		void simple::build(::scene *scene) {
			rd = new raydata(rc->resolution());
			sd = new scenedata;

			binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on> bvh_rt;
			if(bvh_type == "sah") bvh_rt.binary_split_type = bvh_rt.sah;
			else if(bvh_type == "sm") bvh_rt.binary_split_type = bvh_rt.sm;
			else if(bvh_type == "om") bvh_rt.binary_split_type = bvh_rt.om;
			bvh_rt.max_triangles_per_node = bvh_max_tris_per_node;
			bvh_rt.build(scene);

			std::vector<wf::cuda::simpleBVHNode> nodes;
			for(const auto& n : bvh_rt.nodes) {
				wf::cuda::simpleBVHNode node(n);
				nodes.push_back(node);
			}
			assert(nodes.size() == bvh_rt.nodes.size());
			bvh_index.upload(bvh_rt.index);
			bvh_nodes.upload(nodes);

			auto *rt = dynamic_cast<batch_rt*>(rc->scene.batch_rt);
			assert(rt != nullptr);
			sd->upload(scene);
			std::cout << "upload done" << std::endl;
		}

		void simple::compute_hit(bool anyhit) {
			auto *rt = dynamic_cast<wf::cuda::simple*>(rc->scene.batch_rt);
			assert(rt != nullptr);
			int2 resolution{rc->resolution().x, rc->resolution().y};

			simpleTrace<<<NUM_BLOCKS_FOR_RESOLUTION(resolution), DESIRED_BLOCK_SIZE>>>(resolution,
														rd->rays.device_memory,
														sd->vertex_pos.device_memory,
														sd->triangles.device_memory,
														bvh_index.device_memory,
														bvh_nodes.device_memory,
														rd->intersections.device_memory,
														anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError());
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize());
		}
	}
}
