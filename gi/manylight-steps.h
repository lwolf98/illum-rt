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

		virtual void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, per_sample_data<vec3> *le, per_sample_data<int> *vpl_store_offset, compute_light_distribution *light_dist) = 0;
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
			VSD *le = nullptr;
			INT *vpl_store_offset = nullptr;
			LD *light_dist = nullptr;

			bool properly_wired()
			{
				return light_rays && light_throughput && le && vpl_store_offset && light_dist;
			}

			void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, per_sample_data<vec3> *le, per_sample_data<int> *vpl_store_offset, compute_light_distribution *light_dist)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<VSD *>(light_throughput);
				this->le = dynamic_cast<VSD *>(le);
				this->vpl_store_offset = dynamic_cast<INT *>(vpl_store_offset);
				this->light_dist = dynamic_cast<LD *>(light_dist);
			}
		};
	}

	class create_vpls : public step
	{
	public:
		static constexpr char id[] = "create vpls";

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

		virtual void use(raydata *light_rays, per_sample_data<vec3> *light_throughput, vpldata *vpl_store, per_sample_data<int> *vpl_store_offset, int depth) = 0;
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

	class copy_vpls : public step
	{
	public:
		static constexpr char id[] = "copy vpls";

		virtual void use(vpldata *vpl_store, vpldata *vpls, per_sample_data<int> *vpl_count, per_sample_data<float> *scale, int sppx) = 0;
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

			bool properly_wired()
			{
				return vpl_store && vpls && vpl_count && scale;
			}

			void use(vpldata *vpl_store, vpldata *vpls, per_sample_data<int> *vpl_count, per_sample_data<float> *scale, int sppx)
			{
				this->vpl_store = dynamic_cast<VD *>(vpl_store);
				this->vpls = dynamic_cast<VD *>(vpls);
				this->vpl_count = dynamic_cast<INT *>(vpl_count);
				this->scale = dynamic_cast<FLOAT *>(scale);
				this->sppx = sppx;
			}
		};
	}

	/* Integration steps */
	class sample_vpls : public step
	{
	public:
		static constexpr char id[] = "sample vpl rays";

		virtual void use(raydata *camrays, raydata *shadowrays, vpldata *vpls, vpldata *sampled_vpls, per_sample_data<int> *vpl_count,
						per_sample_data<int> *sample_index, int vpls_per_sample, int vpl_offest) = 0;
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
			VD *sampled_vpls = nullptr;
			INT *vpl_count = nullptr;

			INT *sample_index = nullptr;
			int vpls_per_sample = 0;
			int vpl_offset = 0;

			bool properly_wired()
			{
				return camrays && shadowrays && vpls && sampled_vpls && vpl_count && sample_index;
			}

			void use(raydata *camrays, raydata *shadowrays, vpldata *vpls, vpldata *sampled_vpls, per_sample_data<int> *vpl_count,
			per_sample_data<int> *sample_index, int vpls_per_sample, int vpl_offest)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->vpls = dynamic_cast<VD *>(vpls);
				this->sampled_vpls = dynamic_cast<VD *>(sampled_vpls);
				this->vpl_count = dynamic_cast<INT *>(vpl_count);
				this->sample_index = dynamic_cast<INT *>(sample_index);
				this->vpls_per_sample = vpls_per_sample;
				this->vpl_offset = vpl_offest;
			}
		};
	}

	class integrate_vpl_samples : public step
	{
	public:
		static constexpr char id[] = "integrate vpl samples";

		virtual void use(raydata *camrays, raydata *shadowrays, vpldata *sampled_vpls, per_sample_data<float> *scale,
						per_sample_data<int> *sample_index, int vpls_per_sample, int vpl_offest) = 0;
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
			VD *sampled_vpls = nullptr;
			FLOAT *scale = nullptr;

			INT *sample_index = nullptr;
			int vpls_per_sample = 0;
			int vpl_offset = 0;

			bool properly_wired()
			{
				return camrays && shadowrays && sampled_vpls && scale && sample_index;
			}

			void use(raydata *camrays, raydata *shadowrays, vpldata *sampled_vpls, per_sample_data<float> *scale,
			per_sample_data<int> *sample_index, int vpls_per_sample, int vpl_offest)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->sampled_vpls = dynamic_cast<VD *>(sampled_vpls);
				this->scale = dynamic_cast<FLOAT *>(scale);
				this->sample_index = dynamic_cast<INT *>(sample_index);
				this->vpls_per_sample = vpls_per_sample;
				this->vpl_offset = vpl_offest;
			}
		};
	}
}