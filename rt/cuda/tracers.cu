#include "tracers.h"

#include "platform.h"

#include "libgi/timer.h"

#include <iostream>

#include "kernels.h"
#include "cuda-helpers.h"

namespace wf {
	namespace cuda {

		void simple_rt::build(::scene *scene) {
			rd = new raydata(rc->resolution());
			sd = new scenedata;

			binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on> bvh_rt;
			if (bvh_type == "sah") bvh_rt.binary_split_type = bvh_rt.sah;
			else if (bvh_type == "sm") bvh_rt.binary_split_type = bvh_rt.sm;
			else if (bvh_type == "om") bvh_rt.binary_split_type = bvh_rt.om;
			bvh_rt.max_triangles_per_node = bvh_max_tris_per_node;
			bvh_rt.build(scene);

			std::vector<wf::cuda::simple_bvh_node> nodes;
			for (const auto &n : bvh_rt.nodes) {
				wf::cuda::simple_bvh_node node(n);
				nodes.push_back(node);
			}
			assert(nodes.size() == bvh_rt.nodes.size());
			bvh_index.upload(bvh_rt.index);
			bvh_nodes.upload(nodes);

			sd->upload(scene);
			std::cout << "upload done" << std::endl;
		}

		void simple_rt::compute_hit(bool anyhit) {
			int2 resolution{rc->resolution().x, rc->resolution().y};
			simple_trace<<<NUM_BLOCKS_FOR_RESOLUTION(resolution), DESIRED_BLOCK_SIZE>>>(resolution,
																						rd->rays.device_memory,
																						sd->vertex_pos.device_memory,
																						sd->triangles.device_memory,
																						bvh_index.device_memory,
																						bvh_nodes.device_memory,
																						rd->intersections.device_memory,
																						anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError(), "");
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		}


		void ifif::compute_hit(bool anyhit) {
			int2 resolution{rc->resolution().x, rc->resolution().y};
			ifif_trace<<<NUM_BLOCKS_FOR_RESOLUTION(resolution), DESIRED_BLOCK_SIZE>>>(resolution,
																					  rd->rays.device_memory,       rd->rays.tex,
																					  sd->vertex_pos.device_memory, sd->vertex_pos.tex,
																					  sd->triangles.device_memory,  sd->triangles.tex,
																					  bvh_index.device_memory,      bvh_index.tex,
																					  bvh_nodes.device_memory,      bvh_nodes.tex,
																					  rd->intersections.device_memory,
																					  anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError(), "");
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		}
		
		
		void whilewhile::compute_hit(bool anyhit) {
			int2 resolution{rc->resolution().x, rc->resolution().y};
			whilewhile_trace<<<NUM_BLOCKS_FOR_RESOLUTION(resolution), DESIRED_BLOCK_SIZE>>>(resolution,
																							rd->rays.device_memory,       rd->rays.tex,
																							sd->vertex_pos.device_memory, sd->vertex_pos.tex,
																							sd->triangles.device_memory,  sd->triangles.tex,
																							pf->rt->bvh_index.device_memory,  pf->rt->bvh_index.tex,
																							pf->rt->bvh_nodes.device_memory,  pf->rt->bvh_nodes.tex,
																							rd->intersections.device_memory,
																							anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError(), "");
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		}
		
		
		void dynamicwhilewhile::compute_hit(bool anyhit) {
			// dynamicwhilewhile_trace<<<DESIRED_BLOCKS_COUNT, DESIRED_BLOCK_SIZE>>>(
			// dynamicwhilewhile-Kernel uses 48 Registers instead of 40, so run one less warp than usual for best occupation
			dynamicwhilewhile_trace<<<DESIRED_BLOCKS_COUNT, dim3(WARPSIZE, DESIRED_WARPS_PER_BLOCK-1, 1)>>>(rc->resolution().x * rc->resolution().y,
																											rd->rays.device_memory,       rd->rays.tex,
																											sd->vertex_pos.device_memory, sd->vertex_pos.tex,
																											sd->triangles.device_memory,  sd->triangles.tex,
																											pf->rt->bvh_index.device_memory,  pf->rt->bvh_index.tex,
																											pf->rt->bvh_nodes.device_memory,  pf->rt->bvh_nodes.tex,
																											rd->intersections.device_memory,
																											anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError(), "");
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		}
		
		
		void speculativewhilewhile::compute_hit(bool anyhit) {
			int2 resolution{rc->resolution().x, rc->resolution().y};
			speculativewhilewhile_trace<<<NUM_BLOCKS_FOR_RESOLUTION(resolution), DESIRED_BLOCK_SIZE>>>(resolution,
																									   rd->rays.device_memory,       rd->rays.tex,
																									   sd->vertex_pos.device_memory, sd->vertex_pos.tex,
																									   sd->triangles.device_memory,  sd->triangles.tex,
																									   pf->rt->bvh_index.device_memory,  pf->rt->bvh_index.tex,
																									   pf->rt->bvh_nodes.device_memory,  pf->rt->bvh_nodes.tex,
																									   rd->intersections.device_memory,
																									   anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError(), "");
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		}
		
		
		void persistentifif::compute_hit(bool anyhit) {
			persistentifif_trace<<<DESIRED_BLOCKS_COUNT, DESIRED_BLOCK_SIZE>>>(rc->resolution().x*rc->resolution().y,
																			   rd->rays.device_memory,       rd->rays.tex,
																			   sd->vertex_pos.device_memory, sd->vertex_pos.tex,
																			   sd->triangles.device_memory,  sd->triangles.tex,
																			   pf->rt->bvh_index.device_memory,  pf->rt->bvh_index.tex,
																			   pf->rt->bvh_nodes.device_memory,  pf->rt->bvh_nodes.tex,
																			   rd->intersections.device_memory,
																			   anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError(), "");
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		}
		
		
		void persistentspeculativewhilewhile::compute_hit(bool anyhit) {
			persistentspeculativewhilewhile_trace<<<DESIRED_BLOCKS_COUNT, DESIRED_BLOCK_SIZE>>>(rc->resolution().x * rc->resolution().y,
																								rd->rays.device_memory,       rd->rays.tex,
																								sd->vertex_pos.device_memory, sd->vertex_pos.tex,
																								sd->triangles.device_memory,  sd->triangles.tex,
																								pf->rt->bvh_index.device_memory,  pf->rt->bvh_index.tex,
																								pf->rt->bvh_nodes.device_memory,  pf->rt->bvh_nodes.tex,
																								rd->intersections.device_memory,
																								anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError(), "");
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		}
		
		
		void persistentwhilewhile::compute_hit(bool anyhit) {
			persistentwhilewhile_trace<<<DESIRED_BLOCKS_COUNT, DESIRED_BLOCK_SIZE>>>(rc->resolution().x*rc->resolution().y,
																					 rd->rays.device_memory,       rd->rays.tex,
																					 sd->vertex_pos.device_memory, sd->vertex_pos.tex,
																					 sd->triangles.device_memory,  sd->triangles.tex,
																					 pf->rt->bvh_index.device_memory,  pf->rt->bvh_index.tex,
																					 pf->rt->bvh_nodes.device_memory,  pf->rt->bvh_nodes.tex,
																					 rd->intersections.device_memory,
																					 anyhit);
			CHECK_CUDA_ERROR(cudaGetLastError(), "");
  			CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		}

	}
}
