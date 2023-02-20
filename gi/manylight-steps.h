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

		virtual void use(raydata *light_rays, vec3 *light_throughput, vec3 *le) = 0;
	};
	namespace wire
	{

		template <typename RD, typename V>
		class sample_v_0s : public wf::sample_v_0s
		{
		public:
			using wf::sample_v_0s::sample_v_0s;
			RD *light_rays = nullptr;
			V *light_throughput = nullptr;
			V *le = nullptr;

			bool properly_wired()
			{
				return light_rays && light_throughput && le;
			}

			void use(raydata *light_rays, vec3 *light_throughput, vec3 *le)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<V *>(light_throughput);
				this->le = dynamic_cast<V *>(le);
			}
		};
	}

	class create_vpls : public step
	{
	public:
		static constexpr char id[] = "create vpls";

		virtual void use(raydata *light_rays, vec3 *light_throughput, vpl *vpl_store_lane) = 0;
	};
	namespace wire
	{

		template <typename RD, typename LT, typename VL>
		class create_vpls : public wf::create_vpls
		{
		public:
			using wf::create_vpls::create_vpls;
			RD *light_rays = nullptr;
			LT *light_throughput = nullptr;
			VL *vpl_store_lane = nullptr;

			bool properly_wired()
			{
				return light_rays && light_throughput && vpl_store_lane;
			}

			void use(raydata *light_rays, vec3 *light_throughput, vpl *vpl_store_lane)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<LT *>(light_throughput);
				this->vpl_store_lane = dynamic_cast<VL *>(vpl_store_lane);
			}
		};
	}

	class russian_roulette : public step
	{
	public:
		static constexpr char id[] = "russian roulette";

		virtual void use(raydata *light_rays, vec3 *light_throughput, vec3 *le) = 0;
	};
	namespace wire
	{

		template <typename RD, typename V>
		class russian_roulette : public wf::russian_roulette
		{
		public:
			using wf::russian_roulette::russian_roulette;
			RD *light_rays = nullptr;
			V *light_throughput = nullptr;
			V *le = nullptr;

			bool properly_wired()
			{
				return light_rays && light_throughput && le;
			}

			void use(raydata *light_rays, vec3 *light_throughput, vec3 *le)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<V *>(light_throughput);
				this->le = dynamic_cast<V *>(le);
			}
		};
	}

	class sample_next_vpls : public step
	{
	public:
		static constexpr char id[] = "sample next vpls";

		virtual void use(raydata *light_rays, vec3 *light_throughput, vpl *vpl_store_lane) = 0;
	};
	namespace wire
	{

		template <typename RD, typename LT, typename VL>
		class sample_next_vpls : public wf::sample_next_vpls
		{
		public:
			using wf::sample_next_vpls::sample_next_vpls;
			RD *light_rays = nullptr;
			LT *light_throughput = nullptr;
			VL *vpl_store_lane = nullptr;

			bool properly_wired()
			{
				return light_rays && light_throughput && vpl_store_lane;
			}

			void use(raydata *light_rays, vec3 *light_throughput, vpl *vpl_store_lane)
			{
				this->light_rays = dynamic_cast<RD *>(light_rays);
				this->light_throughput = dynamic_cast<LT *>(light_throughput);
				this->vpl_store_lane = dynamic_cast<VL *>(vpl_store_lane);
			}
		};
	}

	class copy_vpls : public step
	{
	public:
		static constexpr char id[] = "copy vpls";

		virtual void use(vpl *vpl_store, std::vector<vpl> *vpls) = 0;
	};
	namespace wire
	{

		template <typename VS, typename VEC>
		class copy_vpls : public wf::copy_vpls
		{
		public:
			using wf::copy_vpls::copy_vpls;
			VS *vpl_store = nullptr;
			VEC *vpls = nullptr;

			bool properly_wired()
			{
				return vpl_store && vpls;
			}

			void use(vpl *vpl_store, std::vector<vpl> *vpls)
			{
				this->vpl_store = dynamic_cast<VS *>(vpl_store);
				this->vpls = dynamic_cast<VEC *>(vpls);
			}
		};
	}

	/* Integration steps */
	class sample_vpls : public step
	{
	public:
		static constexpr char id[] = "sample vpl rays";

		virtual void use(raydata *camrays, raydata *shadowrays, std::vector<vpl> *vpls, vpl *sampled_vpls) = 0;
	};
	namespace wire
	{

		template <typename RD, typename VEC, typename VPL>
		class sample_vpls : public wf::sample_vpls
		{
		public:
			using wf::sample_vpls::sample_vpls;
			RD *camrays = nullptr;
			RD *shadowrays = nullptr;
			VEC *vpls = nullptr;
			VPL *sampled_vpls = nullptr;

			bool properly_wired()
			{
				return camrays && shadowrays && vpls && sampled_vpls;
			}

			void use(raydata *camrays, raydata *shadowrays, std::vector<vpl> *vpls, vpl *sampled_vpls)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->vpls = dynamic_cast<VEC *>(vpls);
				this->sampled_vpls = dynamic_cast<VPL *>(sampled_vpls);
			}
		};
	}

	class integrate_vpl_samples : public step
	{
	public:
		static constexpr char id[] = "integrate vpl samples";

		//TODO-ML: per_sample_data should be used for sampled_vpls...
		virtual void use(raydata *camrays, raydata *shadowrays, vpl *sampled_vpls) = 0;
	};
	namespace wire
	{

		template <typename RD, typename VPL>
		class integrate_vpl_samples : public wf::integrate_vpl_samples
		{
		public:
			using wf::integrate_vpl_samples::integrate_vpl_samples;
			RD *camrays = nullptr;
			RD *shadowrays = nullptr;
			VPL *sampled_vpls = nullptr;

			bool properly_wired()
			{
				return camrays && shadowrays && sampled_vpls;
			}

			void use(raydata *camrays, raydata *shadowrays, vpl *sampled_vpls)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->sampled_vpls = dynamic_cast<VPL *>(sampled_vpls);
			}
		};
	}
}