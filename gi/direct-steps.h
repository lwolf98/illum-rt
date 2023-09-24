#pragma once
#include "primary-steps.h"
namespace wf
{
	//Should support camdata and bouncedata being aliases

	class sample_uniform_dir : public step
	{
	public:
		static constexpr char id[] = "sample uniform dir";

		virtual void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf) = 0;
	};
	namespace wire
	{

		template <typename RD, typename PDF>
		class sample_uniform_dir : public wf::sample_uniform_dir
		{
		public:
			using wf::sample_uniform_dir::sample_uniform_dir;
			RD *camdata = nullptr;
			RD *bouncedata = nullptr;
			PDF *pdf = nullptr;

			bool properly_wired()
			{
				return camdata && bouncedata && pdf;
			}

			void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf)
			{
				this->camdata = dynamic_cast<RD *>(camdata);
				this->bouncedata = dynamic_cast<RD *>(bouncedata);
				this->pdf = dynamic_cast<PDF *>(pdf);
			}
		};
	}

	class sample_cos_weighted_dir : public step
	{
	public:
		static constexpr char id[] = "sample cosine distributed dir";

		virtual void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf) = 0;
	};
	namespace wire
	{

		template <typename RD, typename PDF>
		class sample_cos_weighted_dir : public wf::sample_cos_weighted_dir
		{
		public:
			using wf::sample_cos_weighted_dir::sample_cos_weighted_dir;
			RD *camdata = nullptr;
			RD *bouncedata = nullptr;
			PDF *pdf = nullptr;

			bool properly_wired()
			{
				return camdata && bouncedata && pdf;
			}

			void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf)
			{
				this->camdata = dynamic_cast<RD *>(camdata);
				this->bouncedata = dynamic_cast<RD *>(bouncedata);
				this->pdf = dynamic_cast<PDF *>(pdf);
			}
		};
	}

	class integrate_dir_sample : public step
	{
	public:
		static constexpr char id[] = "integrate directional sample";

		virtual void use(raydata *camrays, raydata *shadowrays, per_sample_data<float> *pdf) = 0;
	};
	namespace wire
	{

		template <typename RD, typename PDF>
		class integrate_dir_sample : public wf::integrate_dir_sample
		{
		public:
			using wf::integrate_dir_sample::integrate_dir_sample;
			RD *camrays = nullptr;
			RD *shadowrays = nullptr;
			PDF *pdf = nullptr;

			bool properly_wired()
			{
				return camrays && shadowrays && pdf;
			}

			void use(raydata *camrays, raydata *shadowrays, per_sample_data<float> *pdf)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->pdf = dynamic_cast<PDF *>(pdf);
			}
		};
	}

	class compute_light_distribution : public step
	{
	public:
		static constexpr char id[] = "compute light distribution";
	};

	class sample_light_dir : public step
	{
	public:
		static constexpr char id[] = "sample dir according to light distribution";

		virtual void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf, compute_light_distribution *light_dist, per_sample_data<vec3> *light_col) = 0;
	};
	namespace wire
	{

		template <typename RD, typename PDF, typename LD, typename LC>
		class sample_light_dir : public wf::sample_light_dir
		{
		public:
			using wf::sample_light_dir::sample_light_dir;
			RD *camdata = nullptr;
			RD *bouncedata = nullptr;
			PDF *pdf = nullptr;
			LD *light_dist = nullptr;
			LC *light_col = nullptr;

			bool properly_wired()
			{
				return camdata && bouncedata && pdf && light_dist && light_col;
			}

			void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf, compute_light_distribution *light_dist, per_sample_data<vec3> *light_col)
			{
				this->camdata = dynamic_cast<RD *>(camdata);
				this->bouncedata = dynamic_cast<RD *>(bouncedata);
				this->pdf = dynamic_cast<PDF *>(pdf);
				this->light_dist = dynamic_cast<LD *>(light_dist);
				this->light_col = dynamic_cast<LC *>(light_col);
			}
		};
	}

	class integrate_light_sample : public step
	{
	public:
		static constexpr char id[] = "integrate light sample";

		virtual void use(raydata *camrays, raydata *shadowrays, per_sample_data<float> *pdf, per_sample_data<vec3> *light_col) = 0;
	};
	namespace wire
	{

		template <typename RD, typename PDF, typename LC>
		class integrate_light_sample : public wf::integrate_light_sample
		{
		public:
			using wf::integrate_light_sample::integrate_light_sample;
			RD *camrays = nullptr;
			RD *shadowrays = nullptr;
			PDF *pdf = nullptr;
			LC *light_col = nullptr;

			bool properly_wired()
			{
				return camrays && shadowrays && pdf && light_col;
			}

			void use(raydata *camrays, raydata *shadowrays, per_sample_data<float> *pdf, per_sample_data<vec3> *light_col)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->pdf = dynamic_cast<PDF *>(pdf);
				this->light_col = dynamic_cast<LC *>(light_col);
			}
		};
	}
}