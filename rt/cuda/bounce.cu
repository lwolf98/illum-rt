#include "bounce.h"

#include "libgi/util.h"
#include "libgi/sampling.h"

#include "cuda-operators.h"

#define launch_config NUM_BLOCKS_FOR_RESOLUTION(res), DESIRED_BLOCK_SIZE
namespace wf::cuda {

	__device__ float3 f3(const float4 &v) { return make_float3(v.x, v.y, v.z); }

	__device__ float3 hit_ng(const tri_is &hit, const uint4 &tri, const float4 *vert_norm) {
		float3 a = f3(vert_norm[tri.x]);
		float3 b = f3(vert_norm[tri.y]);
		float3 c = f3(vert_norm[tri.z]);
		return bary_interpol(a, b, c, hit.beta, hit.gamma);
	}

	// 
	// Uniform Sampling
	// 

	namespace k {
		static __device__ bool not_black(float4 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}
		static __global__ void sample_uniform_dir(int2 res, float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
												  uint4 *triangles, float4 *vert_norm, material *materials,
												  float *pdf, float2 *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;
	
			tri_is hit = hits[ray_index];
			float3 w_i { 0,0,0 };
			float3 org { 0,0,0 };
			float tmax = -FLT_MAX;
			if (hit.valid()) {
				uint4 tri = triangles[hit.ref];
				material m = materials[tri.w];
				if (not_black(m.emissive))
					framebuffer[ray_index] = framebuffer[ray_index] + m.emissive; // might be w==0
				else {
					float2 xi = random[ray_index];
					float3 sampled_dir = uniform_sample_hemisphere<float3>(xi);
					float3 ng = hit_ng(hit, tri, vert_norm);
					float3 cam_dir = f3(camrays[ray_index*2 + 1]);
					flip_normals_to_ray(ng, cam_dir);
					w_i = align(sampled_dir, ng);
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
												 pf->sd->triangles.device_memory,
												 pf->sd->vertex_norm.device_memory,
												 pf->sd->materials.device_memory,
												 pdf->data.device_memory,
												 rng.random_numbers);
	}

	// 
	// Cos Sampling
	// 

	namespace k {
		static __global__ void sample_cos_dir(int2 res, float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
											  uint4 *triangles, float4 *vert_norm, material *materials,
											  float *pdf, float2 *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;
	
			tri_is hit = hits[ray_index];
			float3 w_i { 0,0,0 };
			float3 org { 0,0,0 };
			float tmax = -FLT_MAX;
			if (hit.valid()) {
				uint4 tri = triangles[hit.ref];
				material m = materials[tri.w];
				if (not_black(m.emissive))
					framebuffer[ray_index] = framebuffer[ray_index] + m.emissive; // might be w==0
				else {
					float2 xi = random[ray_index];
					float3 sampled_dir = cosine_sample_hemisphere<float3>(xi);
					float3 ng = hit_ng(hit, tri, vert_norm);
					float3 cam_dir = f3(camrays[ray_index*2 + 1]);
					flip_normals_to_ray(ng, cam_dir);
					w_i = align(sampled_dir, ng);
					org = f3(camrays[ray_index*2]) + hit.t * cam_dir;
					tmax = FLT_MAX;
				}
			}
			shadowrays[ray_index*2+0] = make_float4(org.x, org.y, org.z, 0.0001);
			shadowrays[ray_index*2+1] = make_float4(w_i.x, w_i.y, w_i.z, tmax);
			pdf[ray_index] = one_over_pi;
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
											 pf->sd->triangles.device_memory,
											 pf->sd->vertex_norm.device_memory,
											 pf->sd->materials.device_memory,
											 pdf->data.device_memory,
											 rng.random_numbers);
	}

	// 
	// Integration
	// 

	namespace k {

		__device__ float3 albedo(uint4 tri, const tri_is &hit, const material &mat, float2 *vertex_tc) {
			if (mat.albedo_tex > 0) {
				float2 tc = bary_interpol(vertex_tc[tri.x], vertex_tc[tri.y], vertex_tc[tri.z], hit.beta, hit.gamma);
				return f3(tex2D<float4>(mat.albedo_tex, tc.x, tc.y));
			}
			return f3(mat.albedo);
		}

		__device__ float3 lambertian_reflection(float3 w_o, float3 w_i, float3 ns,
												uint4 tri, const tri_is &hit, const material &mat, float2 *vertex_tc) {
			if (!same_hemisphere(w_i, ns)) return make_float3(0,0,0);
			return one_over_pi * albedo(tri, hit, mat, vertex_tc);
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

		
		__device__ float3 gtr_coat_reflection(float3 w_o, float3 w_i, float3 ns,
											  uint4 tri, const tri_is &hit, const material &mat, float2 *vertex_tc) {
			if (!same_hemisphere(ns, w_i)) return make_float3(0,0,0); // should be ng
			const float NdotV = cdot(ns, w_o);
			const float NdotL = cdot(ns, w_i);
			if (NdotV == 0.f || NdotV == 0.f) return make_float3(0,0,0);
			float3 H = (w_o + w_i); normalize(H);
			const float NdotH = cdot(ns, H);
			const float HdotL = cdot(H, w_i);
			const float F = fresnel_dielectric(HdotL, 1.f, mat.ior);
			const float D = ggx_d(NdotH, mat.roughness);
			const float G = ggx_g1(NdotV, mat.roughness) * ggx_g1(NdotL, mat.roughness);
			const float microfacet = (F * D * G) / (4 * abs(NdotV) * abs(NdotL));
			return make_float3(microfacet,microfacet,microfacet);
		}

		__device__ float3 layered_gtr2(float3 w_o, float3 w_i, float3 ns,
									   uint4 tri, const tri_is &hit, const material &mat, float2 *vertex_tc) {
			const float F = fresnel_dielectric(absdot(ns, w_o), 1.0f, mat.ior);
			float3 diff = lambertian_reflection(w_o, w_i, ns, tri, hit, mat, vertex_tc);
			float3 spec = gtr_coat_reflection(w_o, w_i, ns, tri, hit, mat, vertex_tc);
			return (1.0f-F)*diff + F*spec;
		}

		static __global__ void integrate_light(int2 res,
											   float4 *camrays, tri_is *cam_hits,
											   float4 *shadowrays, tri_is *light_hits,
											   float4 *framebuffer,
											   uint4 *triangles, float4 *vert_norm, float2 *vertex_tc, material *materials,
											   float *pdf) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			tri_is hit = cam_hits[ray_index];
			tri_is light_hit = light_hits[ray_index];
			float3 radiance {0,0,0};
			//if (hit.valid() && light_hit.valid()) {
			if (hit.valid() && light_hit.valid()) {
				// light color
				uint4 light_tri = triangles[light_hit.ref];
				material light_mat = materials[light_tri.w];
				float3 brightness = f3(light_mat.emissive);
				// brdf
				float3 w_o = -f3(camrays[ray_index*2+1]);
				float3 w_i = f3(shadowrays[ray_index*2+1]);
				uint4  tri = triangles[hit.ref];
				float3 ng = hit_ng(hit, tri, vert_norm);
				material mat = materials[tri.w];
				float3 f = layered_gtr2(w_o, w_i, ng, tri, hit, mat, vertex_tc);
				// dot
				float cos_theta = cdot(w_i, ng);
				// combine
				radiance = radiance + brightness * f * cos_theta / pdf[ray_index];
			}
			framebuffer[ray_index] = framebuffer[ray_index] + make_float4(radiance.x, radiance.y, radiance.z, 1.0);
		}
	}

	void integrate_light_sample::run() {
		int2 res = frame_res();
		k::integrate_light<<<launch_config>>>(res,
											  camrays->rays.device_memory,
											  camrays->intersections.device_memory,
											  shadowrays->rays.device_memory,
											  shadowrays->intersections.device_memory,
											  camrays->framebuffer.device_memory,
											  pf->sd->triangles.device_memory,
											  pf->sd->vertex_norm.device_memory,
											  pf->sd->vertex_tc.device_memory,
											  pf->sd->materials.device_memory,
											  pdf->data.device_memory);

	}
}
