#pragma once
#include "primary-steps.h"
namespace wf
{
	//Should support camdata and bouncedata being aliases

	class sample_uniform_dir : public step
	{
	public:
		static constexpr char id[] = "sample uniform dir";
		std::string get_id() override { return id; }

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
		std::string get_id() override { return id; }

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
		std::string get_id() override { return id; }

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
		std::string get_id() override { return id; }
	};

	class sample_light_dir : public step
	{
	public:
		static constexpr char id[] = "sample dir according to light distribution";
		std::string get_id() override { return id; }

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
		std::string get_id() override { return id; }

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



	class sample_mis_dir : public step
	{
	public:
		static constexpr char id[] = "sample dir according to MIS";

		virtual void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf_light, compute_light_distribution *light_dist, per_sample_data<vec3> *light_col) = 0;
	};
	namespace wire
	{

		template <typename RD, typename PDF, typename LD, typename LC>
		class sample_mis_dir : public wf::sample_mis_dir
		{
		public:
			using wf::sample_mis_dir::sample_mis_dir;
			RD *camdata = nullptr;
			RD *bouncedata = nullptr;
			PDF *pdf_light = nullptr;
			LD *light_dist = nullptr;
			LC *light_col = nullptr;
			bool is_light_sample = false;

			bool properly_wired()
			{
				return camdata && bouncedata && pdf_light && light_dist && light_col;
			}

			void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf_light, compute_light_distribution *light_dist, per_sample_data<vec3> *light_col)
			{
				this->camdata = dynamic_cast<RD *>(camdata);
				this->bouncedata = dynamic_cast<RD *>(bouncedata);
				this->pdf_light = dynamic_cast<PDF *>(pdf_light);
				this->light_dist = dynamic_cast<LD *>(light_dist);
				this->light_col = dynamic_cast<LC *>(light_col);
			}
		};
	}

	class integrate_mis_sample : public step
	{
	public:
		static constexpr char id[] = "integrate mis sample";

		virtual void use(raydata *camrays, raydata *shadowrays, per_sample_data<float> *pdf_light, compute_light_distribution *light_dist, per_sample_data<vec3> *light_col) = 0;
	};
	namespace wire
	{

		template <typename RD, typename PDF, typename LD, typename LC>
		class integrate_mis_sample : public wf::integrate_mis_sample
		{
		public:
			using wf::integrate_mis_sample::integrate_mis_sample;
			RD *camrays = nullptr;
			RD *shadowrays = nullptr;
			PDF *pdf_light = nullptr;
			LD *light_dist = nullptr;
			LC *light_col = nullptr;
			bool is_light_sample = false;

			bool properly_wired()
			{
				return camrays && shadowrays && pdf_light && light_dist && light_col;
			}

			void use(raydata *camrays, raydata *shadowrays, per_sample_data<float> *pdf_light, compute_light_distribution *light_dist, per_sample_data<vec3> *light_col)
			{
				this->camrays = dynamic_cast<RD *>(camrays);
				this->shadowrays = dynamic_cast<RD *>(shadowrays);
				this->pdf_light = dynamic_cast<PDF *>(pdf_light);
				this->light_dist = dynamic_cast<LD *>(light_dist);
				this->light_col = dynamic_cast<LC *>(light_col);
			}
		};
	}
}