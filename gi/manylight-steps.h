#pragma once
#include "direct-steps.h"
namespace wf
{
	/**
	 * Manylight steps:
	 * prepare:
	 * sample_v_0(w:v0_raydata, w:throughput/Le_v0, (r:survived), (w:obj_paths))
	 * create_vpl(r:vj_raydata, w:throughput, (r:survived), (w:obj_paths))
	 * russian_roulette(w:survived, w:throughput)
	 * copy_vpls(r:vpl_arr, w:vpls)
	 * (debug_write_obj(r:obj_paths))
	 * 
	 * integration:
	 * integrate_vpl_sample(r:camrays, r:shadowrays, r:pdf?, r:vpls)
	*/

	/* Preparation steps */
	class sample_v_0s : public step
	{
	public:
		static constexpr char id[] = "sample v_0 lights";
		std::string get_id() override { return id; }

		virtual void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, per_sample_data<vec3> *last_vpls_pos, per_sample_data<vec3> *le, per_sample_data<int> *vpl_store_offset, compute_light_distribution *light_dist) = 0;
	};
	namespace wire
	{

		template <typename RD, typename VSD, typename INT, typename LD>
		class sample_v_0s : public wf::sample_v_0s
		{
		public:
			using wf::sample_v_0s::sample_v_0s;
			RD *light_rays = nullptr;
			VSD *light_throughput = nullptr;
			VSD *last_vpls_pos = nullptr;
			VSD *le = nullptr;
			INT *vpl_store_offset = nullptr;
			LD *light_dist = nullptr;

			bool properly_wired()
			{
				return light_rays && light_throughput && last_vpls_pos && le && vpl_store_offset && light_dist;
			}

			void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, per_sample_data<vec3> *last_vpls_pos, per_sample_data<vec3> *le, per_sample_data<int> *vpl_store_offset, compute_light_distribution *light_dist)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<VSD *>(light_throughput);
				this->le = dynamic_cast<VSD *>(le);
				this->vpl_store_offset = dynamic_cast<INT *>(vpl_store_offset);
				this->light_dist = dynamic_cast<LD *>(light_dist);
				this->last_vpls_pos = dynamic_cast<VSD *>(last_vpls_pos);
			}
		};
	}

	class create_vpls : public step
	{
	public:
		static constexpr char id[] = "create vpls";
		std::string get_id() override { return id; }

		virtual void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, vpldata *vpl_store, per_sample_data<int> *vpl_store_offset, int depth) = 0;
	};
	namespace wire
	{

		template <typename RD, typename VSD, typename VD, typename INT>
		class create_vpls : public wf::create_vpls
		{
		public:
			using wf::create_vpls::create_vpls;
			RD *light_rays = nullptr;
			VSD *light_throughput = nullptr;
			VD *vpl_store = nullptr;
			INT *vpl_store_offset = nullptr;
			int depth = 0;

			bool properly_wired()
			{
				return light_rays && light_throughput && vpl_store && vpl_store_offset;
			}

			void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, vpldata *vpl_store, per_sample_data<int> *vpl_store_offset, int depth)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<VSD *>(light_throughput);
				this->vpl_store = dynamic_cast<VD *>(vpl_store);
				this->vpl_store_offset = dynamic_cast<INT *>(vpl_store_offset);
				this->depth = depth;
			}
		};
	}

	class russian_roulette : public step
	{
	public:
		static constexpr char id[] = "russian roulette";
		std::string get_id() override { return id; }

		virtual void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, per_sample_data<vec3> *le) = 0;
	};
	namespace wire
	{

		template <typename RD, typename VSD>
		class russian_roulette : public wf::russian_roulette
		{
		public:
			using wf::russian_roulette::russian_roulette;
			RD *light_rays = nullptr;
			VSD *light_throughput = nullptr;
			VSD *le = nullptr;

			bool properly_wired()
			{
				return light_rays && light_throughput && le;
			}

			void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, per_sample_data<vec3> *le)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<VSD *>(light_throughput);
				this->le = dynamic_cast<VSD *>(le);
			}
		};
	}

	class sample_next_vpls : public step
	{
	public:
		static constexpr char id[] = "sample next vpls";
		std::string get_id() override { return id; }

		virtual void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, per_sample_data<vec3> *last_vpls_pos, vpldata *vpl_store, per_sample_data<int> *vpl_store_offset, int depth) = 0;
	};
	namespace wire
	{

		template <typename RD, typename VSD, typename VD, typename INT>
		class sample_next_vpls : public wf::sample_next_vpls
		{
		public:
			using wf::sample_next_vpls::sample_next_vpls;
			RD *light_rays = nullptr;
			VSD *light_throughput = nullptr;
			VSD *last_vpls_pos = nullptr;
			VD *vpl_store = nullptr;
			INT *vpl_store_offset = nullptr;
			int depth = 0;

			bool properly_wired()
			{
				return light_rays && light_throughput && last_vpls_pos && vpl_store && vpl_store_offset;
			}

			void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, per_sample_data<vec3> *last_vpls_pos, vpldata *vpl_store, per_sample_data<int> *vpl_store_offset, int depth)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<VSD *>(light_throughput);
				this->vpl_store = dynamic_cast<VD *>(vpl_store);
				this->vpl_store_offset = dynamic_cast<INT *>(vpl_store_offset);
				this->depth = depth;
				this->last_vpls_pos = dynamic_cast<VSD *>(last_vpls_pos);
			}
		};
	}

	class copy_vpls : public step
	{
	public:
		static constexpr char id[] = "copy vpls";
		std::string get_id() override { return id; }

		virtual void use(vpldata *vpl_store, vpldata *vpls, per_sample_data<int> *vpl_count, per_sample_data<float> *scale, int sppx,
							per_sample_data<int> *sample_index, per_sample_data<int> *vpl_index) = 0;
	};
	namespace wire
	{

		template <typename VD, typename INT, typename FLOAT>
		class copy_vpls : public wf::copy_vpls
		{
		public:
			using wf::copy_vpls::copy_vpls;
			VD *vpl_store = nullptr;
			VD *vpls = nullptr;
			INT *vpl_count = nullptr;
			FLOAT *scale = nullptr;
			int sppx = 0;

			INT *sample_index = nullptr;
			INT *vpl_index = nullptr;

			bool properly_wired()
			{
				return vpl_store && vpls && vpl_count && scale && sample_index && vpl_index;
			}

			void use(vpldata *vpl_store, vpldata *vpls, per_sample_data<int> *vpl_count, per_sample_data<float> *scale, int sppx,
							per_sample_data<int> *sample_index, per_sample_data<int> *vpl_index)
			{
				this->vpl_store = dynamic_cast<VD *>(vpl_store);
				this->vpls = dynamic_cast<VD *>(vpls);
				this->vpl_count = dynamic_cast<INT *>(vpl_count);
				this->scale = dynamic_cast<FLOAT *>(scale);
				this->sppx = sppx;

				this->sample_index = dynamic_cast<INT *>(sample_index);
				this->vpl_index = dynamic_cast<INT *>(vpl_index);
			}
		};
	}

	/* Integration steps */
	class sample_vpls : public step
	{
	public:
		static constexpr char id[] = "sample vpl rays";
		std::string get_id() override { return id; }

		virtual void use(raydata *camrays, raydata *shadowrays, vpldata *vpls, per_sample_data<int> *sampled_vpl_indices, per_sample_data<int> *vpl_count,
						per_sample_data<int> *sample_index, per_sample_data<int> *vpl_index, int vpls_per_sample, int vpl_offest) = 0;
	};
	namespace wire
	{

		template <typename RD, typename VD, typename INT>
		class sample_vpls : public wf::sample_vpls
		{
		public:
			using wf::sample_vpls::sample_vpls;
			RD *camrays = nullptr;
			RD *shadowrays = nullptr;
			VD *vpls = nullptr;
			INT *sampled_vpl_indices = nullptr;
			INT *vpl_count = nullptr;

			INT *sample_index = nullptr;
			INT *vpl_index = nullptr;
			int vpls_per_sample = 0;
			int vpl_offset = 0;

			bool properly_wired()
			{
				return camrays && shadowrays && vpls && sampled_vpl_indices && vpl_count && sample_index && vpl_index;
			}

			void use(raydata *camrays, raydata *shadowrays, vpldata *vpls, per_sample_data<int> *sampled_vpl_indices, per_sample_data<int> *vpl_count,
			per_sample_data<int> *sample_index, per_sample_data<int> *vpl_index, int vpls_per_sample, int vpl_offest)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->vpls = dynamic_cast<VD *>(vpls);
				this->sampled_vpl_indices = dynamic_cast<INT *>(sampled_vpl_indices);
				this->vpl_count = dynamic_cast<INT *>(vpl_count);
				this->sample_index = dynamic_cast<INT *>(sample_index);
				this->vpl_index = dynamic_cast<INT *>(vpl_index);
				this->vpls_per_sample = vpls_per_sample;
				this->vpl_offset = vpl_offest;
			}
		};
	}

	class integrate_vpl_samples : public step
	{
	public:
		static constexpr char id[] = "integrate vpl samples";
		std::string get_id() override { return id; }

		virtual void use(raydata *camrays, raydata *shadowrays, vpldata *vpls, per_sample_data<int> *sampled_vpl_indices, per_sample_data<float> *scale,
						per_sample_data<int> *sample_index, int vpls_per_sample, int vpl_offest, per_sample_data<int> *vpl_index, float G_max) = 0;
	};
	namespace wire
	{

		template <typename RD, typename VD, typename FLOAT, typename INT>
		class integrate_vpl_samples : public wf::integrate_vpl_samples
		{
		public:
			using wf::integrate_vpl_samples::integrate_vpl_samples;
			RD *camrays = nullptr;
			RD *shadowrays = nullptr;
			VD *vpls = nullptr;
			INT *sampled_vpl_indices = nullptr;
			FLOAT *scale = nullptr;

			INT *sample_index = nullptr;
			int vpls_per_sample = 0;
			int vpl_offset = 0;
			float G_max = FLT_MAX;

			INT *vpl_index = nullptr;

			bool properly_wired()
			{
				return camrays && shadowrays && vpls && sampled_vpl_indices && scale && sample_index && vpl_index;
			}

			void use(raydata *camrays, raydata *shadowrays, vpldata *vpls, per_sample_data<int> *sampled_vpl_indices, per_sample_data<float> *scale,
			per_sample_data<int> *sample_index, int vpls_per_sample, int vpl_offest, per_sample_data<int> *vpl_index, float G_max)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->vpls = dynamic_cast<VD *>(vpls);
				this->sampled_vpl_indices = dynamic_cast<INT *>(sampled_vpl_indices);
				this->scale = dynamic_cast<FLOAT *>(scale);
				this->sample_index = dynamic_cast<INT *>(sample_index);
				this->vpls_per_sample = vpls_per_sample;
				this->vpl_offset = vpl_offest;
				this->G_max = G_max;

				this->vpl_index = dynamic_cast<INT *>(vpl_index);
			}
		};
	}
}