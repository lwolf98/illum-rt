#pragma once
#include "direct-steps.h"
namespace wf
{
	class manylight_step : public step
	{
	public:
		static constexpr char id[] = "manylight step";

		virtual void use(raydata *camdata, raydata *bouncedata, per_sample_data<float> *pdf) = 0;
	};
	namespace wire
	{

		template <typename RD, typename PDF>
		class manylight_step : public wf::manylight_step
		{
		public:
			using wf::manylight_step::manylight_step;
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
}