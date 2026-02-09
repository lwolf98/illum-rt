#include "bounce.h"

#include "libgi/util.h"
#include "libgi/sampling.h"

#include "cuda-operators.h"
#include "trace-helper.cuh"

//#define MIS_DBG
//#define MIS_DBG_LIGHT

#define launch_config NUM_BLOCKS_FOR_RESOLUTION(res), DESIRED_BLOCK_SIZE
namespace wf::cuda {

	const float eps = 1e-4f; // see rt.h

	__device__ float3 f3(const float4 &v) { return make_float3(v.x, v.y, v.z); }

	__device__ float3 hit_ns(const tri_is &hit, const uint4 &tri, const float4 *vert_norm) {
		float3 a = f3(vert_norm[tri.x]);
		float3 b = f3(vert_norm[tri.y]);
		float3 c = f3(vert_norm[tri.z]);
		return bary_interpol(a, b, c, hit.beta, hit.gamma);
	}

	struct __align__(16) diff_geom {
		__device__ __inline__ diff_geom(const tri_is &is, const scene_refs *params) {
			if (is.is_tri())
				init_tri(is, params);
			else
				init_custom_prim(is, params);
		}

		float2 tc;
		float3 x;
		float3 ng, ns;
		const wf::cuda::material *mat;

	private:
		__device__ __forceinline__ void init_base(const float3 vertex_pos_a, const float3 vertex_pos_b, const float3 vertex_pos_c,
													const float3 vertex_norm_a, const float3 vertex_norm_b, const float3 vertex_norm_c,
													const float2 vertex_tc_a, const float2 vertex_tc_b, const float2 vertex_tc_c,
													const float2 barycentrics, const material *mat) {

			float alpha = 1.f - barycentrics.x - barycentrics.y;
			x  = alpha * vertex_pos_a  + barycentrics.x * vertex_pos_b  + barycentrics.y * vertex_pos_c;
			tc = alpha * vertex_tc_a   + barycentrics.x * vertex_tc_b   + barycentrics.y * vertex_tc_c;
			ns = alpha * vertex_norm_a + barycentrics.x * vertex_norm_b + barycentrics.y * vertex_norm_c;
			ng = cross(vertex_pos_b-vertex_pos_a, vertex_pos_c-vertex_pos_a);
			normalize(ng);
			this->mat = mat;
		}

		__device__ __forceinline__ void init_tri(const tri_is &is, const scene_refs *params) {
			const unsigned int primitive_index = is.ref();
			const uint4 triangle = params->triangles[primitive_index];
			const float2 barycentrics = {.x = is.beta, .y = is.gamma};
			init_base(
				f3(params->vertex_pos[triangle.x]), f3(params->vertex_pos[triangle.y]), f3(params->vertex_pos[triangle.z]),
				f3(params->vertex_norm[triangle.x]), f3(params->vertex_norm[triangle.y]), f3(params->vertex_norm[triangle.z]),
				params->vertex_tc[triangle.x], params->vertex_tc[triangle.y], params->vertex_tc[triangle.z],
				barycentrics, &params->materials[triangle.w]
			);
		}

		__device__ __forceinline__ void init_custom_prim(const tri_is &is, const scene_refs *params) {
			uint32_t patch_ref = is.ref(); //((uint32_t)-1) - is.ref();
			bool upper = is.is_upper_tri() > 0;
			int32_t subd_quad_ref = is.quad_ref();

			const subd_patch patch = params->patches[patch_ref];
			const uint4 tri = patch.subd_tri(subd_quad_ref, upper);

			const float2 barycentrics = {.x = is.beta, .y = is.gamma};
			init_base(
				f3(params->patch_vertex_pos[tri.x]), f3(params->patch_vertex_pos[tri.y]), f3(params->patch_vertex_pos[tri.z]),
				f3(params->patch_vertex_norm[tri.x]), f3(params->patch_vertex_norm[tri.y]), f3(params->patch_vertex_norm[tri.z]),
				params->patch_vertex_tc[tri.x], params->patch_vertex_tc[tri.y], params->patch_vertex_tc[tri.z],
				barycentrics, &params->materials[tri.w]
			);

			// TODO: keep this assert?
			//assert(tc.x >= 0 && tc.x <= 1);
			//assert(tc.y >= 0 && tc.y <= 1);
		}
	};

	// 
	// Uniform Sampling
	// 

	namespace k {
		__device__ float4 albedo4(const diff_geom &dg, float2 *vertex_tc);
		__device__ float3 albedo(const diff_geom &dg, float2 *vertex_tc);
		__device__ float4 emissive_albedo4(const diff_geom &dg);

		static __device__ bool not_black(float4 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}
		static __device__ bool not_black(float3 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}
		static __global__ void sample_uniform_dir(int2 res, float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
												  const scene_refs *refs,
												  float *pdf, float2 *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;
	
			tri_is hit = hits[ray_index];
			diff_geom dg(hit, refs);
			float3 w_i { 0,0,0 };
			float3 org { 0,0,0 };
			float tmax = -FLT_MAX;
			if (hit.valid()) {
				if (not_black(dg.mat->emissive))
					framebuffer[ray_index] = framebuffer[ray_index] + emissive_albedo4(dg); // might be w==0
				else {
					float2 xi = random[ray_index];
					float3 sampled_dir = uniform_sample_hemisphere<float3>(xi);
					float3 ns = dg.ns;
					float3 cam_dir = f3(camrays[ray_index*2 + 1]);
					flip_normals_to_ray(ns, cam_dir);
					w_i = align(sampled_dir, ns);
					org = f3(camrays[ray_index*2]) + hit.t * cam_dir;
					tmax = FLT_MAX;
				}
			}
			shadowrays[ray_index*2+0] = make_float4(org.x, org.y, org.z, 0.0001);
			shadowrays[ray_index*2+1] = make_float4(w_i.x, w_i.y, w_i.z, tmax);
			pdf[ray_index] = one_over_2pi;
		}
	}

	int2 frame_res() { auto r = rc->resolution(); return {r.x,r.y}; }

	void sample_uniform_dir::run() {
		rng.compute();

		int2 res = frame_res();
		k::sample_uniform_dir<<<launch_config>>>(res,
												 camdata->rays.device_memory,
												 camdata->intersections.device_memory,
												 bouncedata->rays.device_memory,
												 camdata->framebuffer.device_memory,
												 pf->sd->refs.device_memory,
												 pdf->data.device_memory,
												 rng.random_numbers);
	}

	// 
	// Cos Sampling
	// 

	namespace k {
		static __global__ void sample_cos_dir(int2 res, float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
											  scene_refs *refs,
											  float *pdfs, float2 *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;
	
			tri_is hit = hits[ray_index];
			diff_geom dg(hit, refs);
			float3 w_i { 0,0,0 };
			float3 org { 0,0,0 };
			float tmax = -FLT_MAX;
			float pdf = one_over_pi;
			if (hit.valid()) {
				if (not_black(dg.mat->emissive))
					framebuffer[ray_index] = framebuffer[ray_index] + emissive_albedo4(dg); // might be w==0
				else {
					float2 xi = random[ray_index];
					float3 sampled_dir = cosine_sample_hemisphere<float3>(xi);
					float3 ns = dg.ns;
					float3 cam_dir = f3(camrays[ray_index*2 + 1]);
					flip_normals_to_ray(ns, cam_dir);
					w_i = align(sampled_dir, ns);
					pdf *= cdot(w_i, ns);
					org = f3(camrays[ray_index*2]) + hit.t * cam_dir;
					tmax = FLT_MAX;
				}
			}
			shadowrays[ray_index*2+0] = make_float4(org.x, org.y, org.z, 0.0001);
			shadowrays[ray_index*2+1] = make_float4(w_i.x, w_i.y, w_i.z, tmax);
			pdfs[ray_index] = pdf;
		}
	}

	void sample_cos_weighted_dir::run() {
		rng.compute();

		int2 res = frame_res();
		k::sample_cos_dir<<<launch_config>>>(res,
											 camdata->rays.device_memory,
											 camdata->intersections.device_memory,
											 bouncedata->rays.device_memory,
											 camdata->framebuffer.device_memory,
											 pf->sd->refs.device_memory,
											 pdf->data.device_memory,
											 rng.random_numbers);
	}
	
	// 
	// Light Area Sampling
	// 

	namespace k {
		__device__ int lower_bound(int n, float v, float *lights_cdf) {
			int count = n;
			int first = 0;
			int i, step;
			while (count > 0) {
				i = first;
				step = count/2;
				i += step;
				if (lights_cdf[i] < v) {
					first = ++i;
					count -= step+1;
				}
				else
					count = step;
			}
			return i;
		}

		__device__ int sample_index(int n, float *lights_f, float *lights_cdf, float lights_int_1spaced, float xi, float &pdf) {
			int id = lower_bound(n, xi, lights_cdf);
			id = id > 0 ? id-1 : id; // xi==0
			pdf = lights_f[id] / lights_int_1spaced;
			return id;
		}

		__device__ float3 f3(float4 f4) {
			return {f4.x, f4.y, f4.z};
		}
	
		__device__ void sample_Li(int index, tri_is hit, float2 xi, int ray_index,
								  float4 *camrays, uint4 *tri_lights, const scene_refs *refs,
								  float3 &out_dir, float3 &out_pos, float &out_tmax, float3 &out_lcol, float &out_pdf) {
			uint4 l_tri   = tri_lights[index];
			float3 cam_d  = f3(camrays[ray_index*2+1]);
			float3 from   = f3(camrays[ray_index*2+0]) + hit.t * cam_d;
			float2 bc     = uniform_sample_triangle(xi);
			float3 target = f3(bary_interpol(refs->vertex_pos[l_tri.x],  refs->vertex_pos[l_tri.y],  refs->vertex_pos[l_tri.z],  bc.x, bc.y));
			float3 n      = f3(bary_interpol(refs->vertex_norm[l_tri.x], refs->vertex_norm[l_tri.y], refs->vertex_norm[l_tri.z], bc.x, bc.y));
			float3 w_i    = target - from;

			float area  = 0.5f * length(cross(f3(refs->vertex_pos[l_tri.y]-refs->vertex_pos[l_tri.x]),
											  f3(refs->vertex_pos[l_tri.z]-refs->vertex_pos[l_tri.x])));
			material mat = refs->materials[l_tri.w];
			float3 col = f3(mat.emissive);

			float tmax = length(w_i);
			w_i /= tmax;
			tmax -= eps;
			out_pos = from;
			out_dir = w_i;
			out_tmax = tmax;

			float cos_theta_light = dot(n,-w_i);
			if (cos_theta_light <= 0.0) {
				out_lcol = float3{0,0,0};
				out_pdf = 0;
				return;
			}
			out_lcol = col;
			out_pdf = tmax*tmax/(cos_theta_light * area);
			return;
		}


		static __global__ void sample_light(int2 res, float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
											const scene_refs *refs,
											int lights, float *lights_f, float *lights_cdf, float lights_int_1spaced, uint4 *tri_lights, float3 *lightcol, // TODO F4
											float *pdfs, float4 *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;
			tri_is hit = hits[ray_index];
			diff_geom dg(hit, refs); //TODO: maybe don't use diff_geom here when only material is required
			float3 w_i { 0,0,0 };
			float3 org { 0,0,0 };
			float tmax = -FLT_MAX;
			float3 l_col { 0,0,0 };
			float pdf = 0;
			if (hit.valid()) {
				if (not_black(dg.mat->emissive))
					framebuffer[ray_index] = framebuffer[ray_index] + emissive_albedo4(dg); // might be w==0
				else {
					float4 xis = random[ray_index];
					int l_id = sample_index(lights, lights_f, lights_cdf, lights_int_1spaced, xis.z, pdf);
					float a_pdf = 0, r_tm;
					float3 r_d, r_o;
					sample_Li(l_id, hit, {xis.x,xis.y}, ray_index,
							  camrays, tri_lights, refs,
							  r_d, r_o, r_tm, l_col, a_pdf);
					if (not_black(l_col)) {
						w_i = r_d;
						org = r_o;
						tmax = r_tm;
					}
					pdf *= a_pdf;
				}
			}
			shadowrays[ray_index*2+0] = make_float4(org.x, org.y, org.z, 0.0001);
			shadowrays[ray_index*2+1] = make_float4(w_i.x, w_i.y, w_i.z, tmax);
			pdfs[ray_index] = pdf;
			lightcol[ray_index] = l_col;
		}
	}

	
	void sample_light_dir::run() {
		rng.compute();
		int2 res = frame_res();
		k::sample_light<<<launch_config>>>(res,
										   camdata->rays.device_memory,
										   camdata->intersections.device_memory,
										   bouncedata->rays.device_memory,
										   camdata->framebuffer.device_memory,
										   pf->sd->refs.device_memory,
										   light_dist->n,
										   light_dist->f.device_memory,
										   light_dist->cdf.device_memory,
										   light_dist->integral_1spaced,
										   light_dist->tri_lights.device_memory,
										   light_col->data.device_memory,
										   pdf->data.device_memory,
										   rng.random_numbers);
	}



	namespace k {
		static __global__ void sample_mis(int2 res, float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
											const scene_refs *refs,
											int lights, float *lights_f, float *lights_cdf, float lights_int_1spaced, uint4 *tri_lights, float3 *lightcol, // TODO F4
											float *pdfs_light, float *pdfs_other, bool is_light_sample, float4 *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;
			tri_is hit = hits[ray_index];
			diff_geom dg(hit, refs); //TODO: maybe don't use diff_geom here when only material is required
			float3 w_i { 0,0,0 };
			float3 org { 0,0,0 };
			float tmax = -FLT_MAX;
			float3 l_col { 0,0,0 };
			//float pdf = 0;
			float pdf_light = 0;
			float pdf_other = 0;
			if (hit.valid()) {
				if (not_black(dg.mat->emissive))
					framebuffer[ray_index] = framebuffer[ray_index] + emissive_albedo4(dg); // might be w==0
				else {
					if (is_light_sample) {
						float4 xis = random[ray_index];
						int l_id = sample_index(lights, lights_f, lights_cdf, lights_int_1spaced, xis.z, pdf_light);
						float a_pdf = 0, r_tm;
						float3 r_d, r_o;
						sample_Li(l_id, hit, {xis.x,xis.y}, ray_index,
								camrays, tri_lights, refs,
								r_d, r_o, r_tm, l_col, a_pdf);
						if (not_black(l_col)) {
							w_i = r_d;
							org = r_o;
							tmax = r_tm;
						}
						pdf_light *= a_pdf;

						pdf_other = one_over_2pi;
					}
					else {
						//[MIS]...
						//float2 xi = make_float2{random[ray_index].x, random[ray_index.y]};
						float2 xi;
						xi.x = random[ray_index].x;
						xi.y = random[ray_index].y;
						float3 sampled_dir = uniform_sample_hemisphere<float3>(xi);
						float3 ns = dg.ns;

						float3 cam_org = f3(camrays[ray_index*2 + 0]);
						float3 cam_dir = f3(camrays[ray_index*2 + 1]);
						flip_normals_to_ray(ns, cam_dir);
						w_i = align(sampled_dir, ns);
						org = cam_org + hit.t * cam_dir;
						tmax = FLT_MAX;
						pdf_other = one_over_2pi;

						//pdf_light = ...;
					}
				}
			}
			shadowrays[ray_index*2+0] = make_float4(org.x, org.y, org.z, 0.0001);
			shadowrays[ray_index*2+1] = make_float4(w_i.x, w_i.y, w_i.z, tmax);
			pdfs_light[ray_index] = pdf_light;
			pdfs_other[ray_index] = pdf_other;
			lightcol[ray_index] = l_col;
		}
	}

	void sample_mis_dir::run() {
		rng.compute();
		int2 res = frame_res();
		//is_light_sample = !is_light_sample;
		//is_light_sample = true;
#ifndef MIS_DBG
		is_light_sample = !is_light_sample;
#else
	#ifdef MIS_DBG_LIGHT
		is_light_sample = true;
	#else
		is_light_sample = false;
	#endif
#endif
		k::sample_mis<<<launch_config>>>(res,
										   camdata->rays.device_memory,
										   camdata->intersections.device_memory,
										   bouncedata->rays.device_memory,
										   camdata->framebuffer.device_memory,
										   pf->sd->refs.device_memory,
										   light_dist->n,
										   light_dist->f.device_memory,
										   light_dist->cdf.device_memory,
										   light_dist->integral_1spaced,
										   light_dist->tri_lights.device_memory,
										   light_col->data.device_memory,
										   pdf_light->data.device_memory,
										   pdf_other->data.device_memory,
										   is_light_sample,
										   rng.random_numbers);
	}

	// 
	// Integration
	// 

	namespace k {

		__device__ float4 albedo4(const diff_geom &dg, float2 *vertex_tc) {
			if (dg.mat->albedo_tex > 0) {
				return tex2D<float4>(dg.mat->albedo_tex, dg.tc.x, dg.tc.y);
			}
			return dg.mat->albedo;
		}

		__device__ float3 albedo(const diff_geom &dg, float2 *vertex_tc) {
			return f3(albedo4(dg, vertex_tc));
		}

		__device__ float4 emissive_albedo4(const diff_geom &dg) {
			float4 albedo = albedo4(dg, nullptr);
			if(not_black(albedo))	return albedo * dg.mat->emissive;
			else					return dg.mat->emissive;
		}

		__device__ float3 lambertian_reflection(float3 w_o, float3 w_i, const diff_geom &dg, float2 *vertex_tc) {
			if (!same_hemisphere(w_i, dg.ns)) return make_float3(0,0,0);
			return one_over_pi * albedo(dg, vertex_tc);
		}

		#define sqr(x) ((x)*(x))
		__device__ inline float ggx_d(const float NdotH, float roughness) {
			if (NdotH <= 0) return 0.f;
			const float tan2 = tan2_theta(NdotH);
			if (!isfinite(tan2)) return 0.f;
			const float a2 = sqr(roughness);
			return a2 / (pi * sqr(sqr(NdotH)) * sqr(a2 + tan2));
		}

		__device__ inline float ggx_g1(const float NdotV, float roughness) {
			if (NdotV <= 0) return 0.f;
			const float tan2 = tan2_theta(NdotV);
			if (!isfinite(tan2)) return 0.f;
			return 2.f / (1.f + sqrtf(1.f + sqr(roughness) * tan2));
		}
		#undef sqr

		
		__device__ float3 gtr_coat_reflection(float3 w_o, float3 w_i, const diff_geom &dg, float2 *vertex_tc) {
			if (!same_hemisphere(dg.ns, w_i)) return make_float3(0,0,0); // should be ng
			const float NdotV = cdot(dg.ns, w_o);
			const float NdotL = cdot(dg.ns, w_i);
			if (NdotV == 0.f || NdotV == 0.f) return make_float3(0,0,0);
			float3 H = (w_o + w_i); normalize(H);
			const float NdotH = cdot(dg.ns, H);
			const float HdotL = cdot(H, w_i);
			const float F = fresnel_dielectric(HdotL, 1.f, dg.mat->ior);
			const float D = ggx_d(NdotH, dg.mat->roughness);
			const float G = ggx_g1(NdotV, dg.mat->roughness) * ggx_g1(NdotL, dg.mat->roughness);
			const float microfacet = (F * D * G) / (4 * abs(NdotV) * abs(NdotL));
			return make_float3(microfacet,microfacet,microfacet);
		}

		__device__ float3 layered_gtr2(float3 w_o, float3 w_i, const diff_geom &dg, float2 *vertex_tc) {
			const float F = fresnel_dielectric(absdot(dg.ns, w_o), 1.0f, dg.mat->ior);
			float3 diff = lambertian_reflection(w_o, w_i, dg, vertex_tc);
			float3 spec = gtr_coat_reflection(w_o, w_i, dg, vertex_tc);
			return (1.0f-F)*diff + F*spec;
		}

		//TODO: test the modified implementation, also check whether ng/ns is correctly used
		static __global__ void integrate_dir(int2 res,
											 float4 *camrays, tri_is *cam_hits,
											 float4 *shadowrays, tri_is *light_hits,
											 float4 *framebuffer,
											 wf::cuda::scene_refs *refs,
											 float *pdf) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			tri_is hit = cam_hits[ray_index];
			diff_geom dg(hit, refs);
			tri_is light_hit = light_hits[ray_index];
			float3 radiance {0,0,0};
			if (hit.valid() && light_hit.valid()) {
				// light color
				uint4 light_tri = refs->triangles[light_hit.ref()];
				material light_mat = refs->materials[light_tri.w];
				float3 brightness = f3(light_mat.emissive);
				// brdf
				float3 w_o = -f3(camrays[ray_index*2+1]);
				float3 w_i = f3(shadowrays[ray_index*2+1]);
				float3 f = layered_gtr2(w_o, w_i, dg, hit.is_tri() ? refs->vertex_tc : refs->patch_vertex_tc);
				// dot
				float cos_theta = cdot(w_i, dg.ns);
				// combine
				radiance = radiance + brightness * f * cos_theta / pdf[ray_index];
			}
			framebuffer[ray_index] = framebuffer[ray_index] + make_float4(radiance.x, radiance.y, radiance.z, 1.0);
		}
		
		static __global__ void integrate_light(int2 res,
											   float4 *camrays, tri_is *cam_hits,
											   float4 *shadowrays, tri_is *shadow_hits,
											   float4 *framebuffer,
											   wf::cuda::scene_refs *refs,
											   float3 *lightcol, float *pdf) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			tri_is hit = cam_hits[ray_index];
			tri_is shadow_hit = shadow_hits[ray_index];
			float3 radiance {0,0,0};
			float4 shadowray_dir = shadowrays[2*ray_index+1];

			if (hit.valid() && shadowray_dir.w > 0 && !shadow_hit.valid()) {
				diff_geom dg(hit, refs);

				// light color
				float3 brightness = lightcol[ray_index];
				// brdf
				float3 w_o = -f3(camrays[2*ray_index+1]);
				float3 w_i = f3(shadowray_dir);

				float3 f = layered_gtr2(w_o, w_i, dg, hit.is_tri() ? refs->vertex_tc : refs->patch_vertex_tc);
				// dot
				float cos_theta = cdot(w_i, dg.ns);
				// combine
				radiance = radiance + brightness * f * cos_theta / pdf[ray_index];
			}

			framebuffer[ray_index] = framebuffer[ray_index] + make_float4(radiance.x, radiance.y, radiance.z, 1.0);
		}
	}

	void integrate_dir_sample::run() {
		int2 res = frame_res();
		k::integrate_dir<<<launch_config>>>(res,
											camrays->rays.device_memory,
											camrays->intersections.device_memory,
											shadowrays->rays.device_memory,
											shadowrays->intersections.device_memory,
											camrays->framebuffer.device_memory,
											pf->sd->refs.device_memory,
											pdf->data.device_memory);
	}
	
	void integrate_light_sample::run() {
		int2 res = frame_res();
		k::integrate_light<<<launch_config>>>(res,
											  camrays->rays.device_memory,
											  camrays->intersections.device_memory,
											  shadowrays->rays.device_memory,
											  shadowrays->intersections.device_memory,
											  camrays->framebuffer.device_memory,
											  pf->sd->refs.device_memory,
											  light_col->data.device_memory,
											  pdf->data.device_memory);
	}


	namespace k {// Perceptional brightness of a color
		static __device__ inline float luma(const float3& rgb) {
			return dot({0.212671f, 0.715160f, 0.072169f}, rgb);
		}

		static __global__ void integrate_mis(int2 res,
											   float4 *camrays, tri_is *cam_hits,
											   float4 *shadowrays, tri_is *shadow_hits,
											   float4 *framebuffer,
											   wf::cuda::scene_refs *refs,
											   float3 *lightcol, float *pdfs_light, float *pdfs_other, float lights_int_1spaced, bool is_light_sample) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			tri_is hit = cam_hits[ray_index];
			tri_is shadow_hit = shadow_hits[ray_index];
			float3 radiance {0,0,0};
			float4 shadowray_org = shadowrays[2*ray_index+0];
			float4 shadowray_dir = shadowrays[2*ray_index+1];

			float pdf_light = pdfs_light[ray_index];
			float pdf_other = pdfs_other[ray_index];

			float3 brightness;
			float cos_theta;
			float3 f;
			bool integrated = false;

			if (is_light_sample) {
				if (hit.valid() && shadowray_dir.w > 0 && !shadow_hit.valid()) {
					diff_geom dg(hit, refs);

					// light color
					brightness = lightcol[ray_index];
					// brdf
					float3 w_o = -f3(camrays[2*ray_index+1]);
					float3 w_i = f3(shadowray_dir);

					f = layered_gtr2(w_o, w_i, dg, hit.is_tri() ? refs->vertex_tc : refs->patch_vertex_tc);
					// dot
					cos_theta = cdot(w_i, dg.ns);
					// combine
					//radiance = radiance + brightness * f * cos_theta / pdf_light;
					integrated = true;
				}
			}
			else {
				tri_is &light_hit = shadow_hit;
				if (hit.valid() && light_hit.valid()) {
					diff_geom dg(hit, refs);
					diff_geom on_light(light_hit, refs);

					// light color
					uint4 light_tri = refs->triangles[light_hit.ref()];
					material light_mat = refs->materials[light_tri.w];
					brightness = f3(light_mat.emissive);
					// brdf
					float3 w_o = -f3(camrays[2*ray_index+1]);
					float3 w_i = f3(shadowrays[ray_index*2+1]);
					f = layered_gtr2(w_o, w_i, dg, hit.is_tri() ? refs->vertex_tc : refs->patch_vertex_tc);
					// dot
					cos_theta = cdot(w_i, dg.ns);
					// combine
					//radiance = radiance + brightness * f * cos_theta / pdf[ray_index];

					// Power:
					float area  = 0.5f * length(cross(f3(refs->vertex_pos[light_tri.y]-refs->vertex_pos[light_tri.x]),
													f3(refs->vertex_pos[light_tri.z]-refs->vertex_pos[light_tri.x])));
					float3 power = brightness * area;
					
					// PDF tri light:
					float d = length(on_light.x - f3(shadowray_org));
					float cos_theta_light = dot(on_light.ns, -f3(shadowray_dir));
					//if (cos_theta_light <= 0.0f) return 0.0f;
					float pdf = cos_theta <= 0 ? 0.f : d*d/(cos_theta_light*area);


					//pdf_light = power.x; //luma(tl.power()) / rc->scene.light_distribution->integral();
					pdf_light = luma(power) / lights_int_1spaced;
					pdf_light *= pdf; //tl.pdf(light_ray, hit_geom);
					integrated = true;
				}
			}

			if (integrated) {
				#ifndef MIS_DBG
				float pdf_mis = 0.5 * (pdf_light + pdf_other);
				#else
				#ifdef MIS_DBG_LIGHT
				float pdf_mis = pdf_light;
				#else
				float pdf_mis = pdf_other;
				#endif
				#endif
				radiance = radiance + brightness * f * cos_theta / pdf_mis;
			}

			framebuffer[ray_index] = framebuffer[ray_index] + make_float4(radiance.x, radiance.y, radiance.z, 1.0);
		}
	}

	void integrate_mis_sample::run() {
		int2 res = frame_res();
		//is_light_sample = !is_light_sample;
		//is_light_sample = true;
#ifndef MIS_DBG
		is_light_sample = !is_light_sample;
#else
	#ifdef MIS_DBG_LIGHT
		is_light_sample = true;
	#else
		is_light_sample = false;
	#endif
#endif
		k::integrate_mis<<<launch_config>>>(res,
											  camrays->rays.device_memory,
											  camrays->intersections.device_memory,
											  shadowrays->rays.device_memory,
											  shadowrays->intersections.device_memory,
											  camrays->framebuffer.device_memory,
											  pf->sd->refs.device_memory,
											  light_col->data.device_memory,
											  pdf_light->data.device_memory,
											  pdf_other->data.device_memory,
											  light_dist->integral_1spaced,
											  is_light_sample);
	}
}
