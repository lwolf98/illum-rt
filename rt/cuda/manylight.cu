#include <cub/cub.cuh>

#include "manylight.h"

//#include "bounce.h"
#include "libgi/util.h"
#include "libgi/sampling.h"
#include "libgi/objdraw.h"

#include "cuda-operators.h"

#define launch_config NUM_BLOCKS_FOR_RESOLUTION(res), DESIRED_BLOCK_SIZE
namespace wf::cuda {
	static __device__ float3 hit_ng(const tri_is &hit, const uint4 &tri, const float4 *vert_norm);
	static __device__ float3 f3(const float4 &v);

	static void vpl_stats(const vpldata *vpls, const int size) {
		vec4 col(0);
		vec4 pos(0);
		vec3 normal(0);
		//vec3 normal(0);
		//for (auto v : vpls) {
		for (int i = 0; i < size; i++) {
			col.x += vpls->col.host_data[i].x;
			col.y += vpls->col.host_data[i].y;
			col.z += vpls->col.host_data[i].z;
			col.w += vpls->col.host_data[i].w;
			pos.x += vpls->pos.host_data[i].x;
			pos.y += vpls->pos.host_data[i].y;
			pos.z += vpls->pos.host_data[i].z;
			pos.w += vpls->pos.host_data[i].w;
			//normal += v.normal;
			//if (col.r != col.r)
			//	std::cout << "NAN: " << v.col << std::endl;
		}
		col = col * (1.f/size);
		pos = pos * (1.f/size);
		//normal /= size;

		std::cout << "VPLs: " << size << std::endl;
		//std::cout << "col: " << col << ", pos: " << pos << ", normal: " << normal << std::endl;
		std::cout << "col: " << col.x << "|" << col.y << "|" << col.z << 
					", pos: " << pos.x << "|" << pos.y << "|" << pos.z <<  std::endl;
	}

	static __device__ void vpl_stats_device(const tri_is *vpl_is, const float4 *vpl_w_in, const int max_size, const uint4 *tris, const float4 *vert_normals) {
		float3 normal {0,0,0};
		float3 normal_flipped {0,0,0};
		//vec3 normal(0);
		//for (auto v : vpls) {
		int count = 0;
		for (int i = 0; i < max_size; i++) {
			if (!vpl_is[i].valid()) continue;
			uint4 tri = tris[vpl_is[i].ref];
			float3 cur_normal = hit_ng(vpl_is[i], tri, vert_normals);
			//printf("cur_normal: %f|%f|%f\n", cur_normal.x, cur_normal.y, cur_normal.z);
			//printf("w_in: %f|%f|%f\n", vpl_w_in[i].x, vpl_w_in[i].y, vpl_w_in[i].z);
			normal.x += cur_normal.x;
			normal.y += cur_normal.y;
			normal.z += cur_normal.z;

			flip_normals_to_ray(cur_normal, f3(vpl_w_in[i]));
			normal_flipped.x += cur_normal.x;
			normal_flipped.y += cur_normal.y;
			normal_flipped.z += cur_normal.z;

			count++;
		}
		normal = normal * (1.f/count);
		normal_flipped = normal_flipped * (1.f/count);

		//std::cout << "VPLs: " << size << std::endl;
		//std::cout << "normal: " << normal.x << "|" << normal.y << "|" << normal.z <<  std::endl;
		printf("Count in vpl_ststs_device: %d\n", count);
		printf("normal: %f|%f|%f\n", normal.x, normal.y, normal.z);
		printf("normal_flipped: %f|%f|%f\n", normal_flipped.x, normal_flipped.y, normal_flipped.z);
	}
	

	/* util functions (copied from bounce.cu)*/
	//TODO-ML: use util functions centralized; (frame_res copied from bounce.cu)
	const float eps = 1e-4f; // see rt.h
	static int2 frame_res() { auto r = rc->resolution(); return {r.x,r.y}; }
	
	static __device__ float3 f3(const float4 &v) { return make_float3(v.x, v.y, v.z); }

	static __device__ float3 hit_ng(const tri_is &hit, const uint4 &tri, const float4 *vert_norm) {
		float3 a = f3(vert_norm[tri.x]);
		float3 b = f3(vert_norm[tri.y]);
		float3 c = f3(vert_norm[tri.z]);
		return bary_interpol(a, b, c, hit.beta, hit.gamma);
	}

	namespace k {
		static __device__ bool not_black(float4 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}
		static __device__ bool not_black(float3 c) {
			return c.x != 0 || c.y != 0 || c.z != 0;
		}

		// BRDF / reflection
		static __device__ float3 albedo(uint4 tri, const tri_is &hit, const material &mat, float2 *vertex_tc) {
			if (mat.albedo_tex > 0) {
				float2 tc = bary_interpol(vertex_tc[tri.x], vertex_tc[tri.y], vertex_tc[tri.z], hit.beta, hit.gamma);
				return f3(tex2D<float4>(mat.albedo_tex, tc.x, tc.y));
			}
			return f3(mat.albedo);
		}

		static __device__ float3 lambertian_reflection(float3 w_o, float3 w_i, float3 ns,
												uint4 tri, const tri_is &hit, const material &mat, float2 *vertex_tc) {
			if (!same_hemisphere(w_i, ns)) return make_float3(0,0,0);
			return one_over_pi * albedo(tri, hit, mat, vertex_tc);
		}

		#define sqr(x) ((x)*(x))
		static __device__ inline float ggx_d(const float NdotH, float roughness) {
			if (NdotH <= 0) return 0.f;
			const float tan2 = tan2_theta(NdotH);
			if (!isfinite(tan2)) return 0.f;
			const float a2 = sqr(roughness);
			return a2 / (pi * sqr(sqr(NdotH)) * sqr(a2 + tan2));
		}

		static __device__ inline float ggx_g1(const float NdotV, float roughness) {
			if (NdotV <= 0) return 0.f;
			const float tan2 = tan2_theta(NdotV);
			if (!isfinite(tan2)) return 0.f;
			return 2.f / (1.f + sqrtf(1.f + sqr(roughness) * tan2));
		}
		#undef sqr

		static __device__ float3 gtr_coat_reflection(float3 w_o, float3 w_i, float3 ns,
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

		static __device__ float3 layered_gtr2(float3 w_o, float3 w_i, float3 ns,
									   uint4 tri, const tri_is &hit, const material &mat, float2 *vertex_tc) {
			const float F = fresnel_dielectric(absdot(ns, w_o), 1.0f, mat.ior);
			float3 diff = lambertian_reflection(w_o, w_i, ns, tri, hit, mat, vertex_tc);
			float3 spec = gtr_coat_reflection(w_o, w_i, ns, tri, hit, mat, vertex_tc);
			return (1.0f-F)*diff + F*spec;
		}

		// light sampling
		static __device__ int lower_bound(int n, float v, float *lights_cdf) {
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

		static __device__ int sample_index(int n, float *lights_f, float *lights_cdf, float lights_int_1spaced, float xi, float &pdf) {
			int id = lower_bound(n, xi, lights_cdf);
			id = id > 0 ? id-1 : id; // xi==0
			pdf = lights_f[id] / lights_int_1spaced;
			return id;
		}

		static __device__ float3 f3(float4 f4) {
			return {f4.x, f4.y, f4.z};
		}
	
		static __device__ void sample_Li(int index, tri_is hit, float2 xi, int ray_index,
								  float4 *camrays, uint4 *tri_lights, float4 *vert_pos, float4 *vert_norm, material *materials,
								  float3 &out_dir, float3 &out_pos, float &out_tmax, float3 &out_lcol, float &out_pdf) {
			uint4 l_tri   = tri_lights[index];
			float3 cam_d  = f3(camrays[ray_index*2+1]);
			float3 from   = f3(camrays[ray_index*2+0]) + hit.t * cam_d;
			float2 bc     = uniform_sample_triangle(xi);
			float3 target = f3(bary_interpol(vert_pos[l_tri.x],  vert_pos[l_tri.y],  vert_pos[l_tri.z],  bc.x, bc.y));
			float3 n      = f3(bary_interpol(vert_norm[l_tri.x], vert_norm[l_tri.y], vert_norm[l_tri.z], bc.x, bc.y));
			float3 w_i    = target - from;

			float area  = 0.5f * length(cross(f3(vert_pos[l_tri.y]-vert_pos[l_tri.x]),
											  f3(vert_pos[l_tri.z]-vert_pos[l_tri.x])));
			material mat = materials[l_tri.w];
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
	
		static __device__ void sample_Le(int index, float2 xi_pos, float2 xi_dir,
								  uint4 *tri_lights, float4 *vert_pos, float4 *vert_norm, material *materials,
								  float3 &out_dir, float3 &out_pos, float &out_tmax, float3 &out_lcol, float3 &out_normal, float &out_pdf) {
			uint4 l_tri   = tri_lights[index];
			//float3 cam_d  = f3(camrays[ray_index*2+1]);
			//float3 from   = f3(camrays[ray_index*2+0]) + hit.t * cam_d;
			float2 bc     = uniform_sample_triangle(xi_pos);
			float3 target = f3(bary_interpol(vert_pos[l_tri.x],  vert_pos[l_tri.y],  vert_pos[l_tri.z],  bc.x, bc.y));
			float3 n      = f3(bary_interpol(vert_norm[l_tri.x], vert_norm[l_tri.y], vert_norm[l_tri.z], bc.x, bc.y));
			//float3 w_i    = target - from;

			float area  = 0.5f * length(cross(f3(vert_pos[l_tri.y]-vert_pos[l_tri.x]),
											  f3(vert_pos[l_tri.z]-vert_pos[l_tri.x])));
			float pdf_pos = 1.f/area;
			material mat = materials[l_tri.w];
			//float3 col = f3(mat.emissive);
			// testing
			float3 col = f3(mat.emissive);

			// Sample w:
			float3 w_tan = cosine_sample_hemisphere<float3>(xi_dir);
			float3 w = align(w_tan, n);
			//float cos_theta = cdot(w, n);
			float cos_theta = w_tan.z;
			float pdf_dir = cosine_hemisphere_pdf(cos_theta);

			out_pos = target;
			out_dir = w;
			out_normal = n;
			out_tmax = FLT_MAX; //TODO-ML: correct?

			if (pdf_pos*pdf_dir <= 0) {
				out_lcol = float3{0,0,0};
				out_pdf = 0;
				//out_tmax = -FLT_MAX;
				return;
			}

			out_lcol = col;
			out_pdf = pdf_pos*pdf_dir;
			//out_tmax = FLT_MAX; //TODO-ML: correct?
		}
	}

	/* frame preparation */
	namespace k {
		static __global__ void sample_v_0s(int2 res, float4 *light_rays,
										   float3 *light_throughput, float3 *le, int *vpl_store_offset,
										   uint4 *triangles, float4 *vert_pos, float4 *vert_norm, material *materials,
										   int lights, float *lights_f, float *lights_cdf, float lights_int_1spaced, uint4 *tri_lights,
										   float4 *random_dir, float *random_light) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			//bool test_call = not_black(light_rays[ray_index]);
			//bool test_call = test_funct();
			//f3(light_rays[ray_index]);
			if (x >= res.x || y >= res.y)
				return;
			
			float xis_light = random_light[ray_index];
			float pdf_l = 0;
			int l_id = sample_index(lights, lights_f, lights_cdf, lights_int_1spaced, xis_light, pdf_l);

			float3 Le_v_0 { 0,0,0 };
			float4 xis_dir = random_dir[ray_index];
			float3 r_d, r_o;
			float3 normal_v_0;
			float pdf_Le = 0, r_tm;
			sample_Le(l_id, {xis_dir.z,xis_dir.w}, {xis_dir.x,xis_dir.y},
							  tri_lights, vert_pos, vert_norm, materials,
							  r_d, r_o, r_tm, Le_v_0, normal_v_0, pdf_Le);
			float pdf_v_0 = pdf_l * pdf_Le;

			// Setup the throughput for VPL v_1
			float D_v_0 = cdot(normal_v_0, r_d); //D_v_0(v_1)
			//float3 throughput { 1, 1, 1 };
			//throughput *= Le_v_0 * D_v_0 / pdf_v_0; //Attention: throughput contains Le value
			float3 throughput = Le_v_0 * D_v_0 / pdf_v_0; //Attention: throughput contains Le value

			light_rays[ray_index*2+0] = make_float4(r_o.x, r_o.y, r_o.z, 0.0001f); //TODO-ML: What is that 0.0001?
			light_rays[ray_index*2+1] = make_float4(r_d.x, r_d.y, r_d.z, r_tm);
			light_throughput[ray_index] = throughput;
			le[ray_index] = Le_v_0;

			//Initialize VPL store offset
			//vpl_store_offset[0] = 0;

			// testing
			//light_throughput[ray_index] = Le_v_0; //{1.f,0,1.f};
		}
	}

	void sample_v_0s::run() {
		rng_light.compute();
		rng_dir.compute();
		int2 res = {light_rays->w, light_rays->h};
		k::sample_v_0s<<<launch_config>>>(res,
										  light_rays->rays.device_memory,
										  light_throughput->data.device_memory,
										  le->data.device_memory,
										  vpl_store_offset->data.device_memory,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_pos.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->materials.device_memory,
										  light_dist->n,
										  light_dist->f.device_memory,
										  light_dist->cdf.device_memory,
										  light_dist->integral_1spaced,
										  light_dist->tri_lights.device_memory,
										  rng_dir.random_numbers,
										  rng_light.random_numbers);
	}

	namespace k {
		static __global__ void create_vpls(int2 res, float4 *light_rays, tri_is *hits,
										   float3 *light_throughput,
										   float4 *vpl_col, float4 *vpl_pos, float4 *vpl_w_in, tri_is *vpl_is,
										   int *vpl_store_offset, int depth,
										   uint4 *triangles, float4 *vert_norm, material *materials) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;
			
			float4 light_ray_org = light_rays[ray_index*2+0];
			float4 light_ray_dir = light_rays[ray_index*2+1];

			int offset = depth*res.x;
			//if (light_ray_dir == make_float4(0, 0, 0, 0)) {
			/*if (light_ray_dir.x == 0 && light_ray_dir.y == 0 && light_ray_dir.z == 0) {
				printf("dir 0 triggered\n");
				//TODO-ML: invalid VPL handling
				//vpl_col[ray_index+offset] = make_float4(col.x, col.y, col.z, 0.f);
				//vpl_pos[ray_index+offset] = pos;
				//vpl_w_in[ray_index+offset] = w_in;
				tri_is invalid_is(FLT_MAX, 0, 0, 0);
				vpl_is[ray_index+offset] = invalid_is;

				return;
			}*/

			tri_is hit = hits[ray_index];
			float3 col = light_throughput[ray_index];
			//float3 normalized_dir = f3(light_ray_dir);
			//normalize(normalized_dir);
			float4 pos = light_ray_org + hit.t * light_ray_dir;
			//float4 pos = light_ray_org + hit.t * make_float4(normalized_dir.x, normalized_dir.y, normalized_dir.z, 0.f);
			//float3 normal = hit.ns; //hit_ng(hit, tri, vert_norm)
			float4 w_in = light_ray_dir;

			//TODO-ML: testing col = 0 handling
			//if (col.x == 0 && col.y == 0 && col.z == 0) {
			if (!hit.valid() || col.x == 0.f && col.y == 0.f && col.z == 0.f) {
				//printf("Col 0 triggered\n");
				vpl_col[ray_index+offset] = make_float4(col.x, col.y, col.z, -1); //make_float4(0, 0, 0, -1);
				vpl_pos[ray_index+offset] = make_float4(pos.x, pos.y, pos.z, -1);
				vpl_w_in[ray_index+offset] = make_float4(w_in.x, w_in.y, w_in.z, -1);
				//vpl_is[ray_index+offset] = hit;
				tri_is invalid_is(FLT_MAX, 0, 0, 0);
				vpl_is[ray_index+offset] = invalid_is;
				//light_rays[ray_index*2+1] = {0,0,0,-1.f};

				if (x < 20)
				//if (x == 16)
					printf("CREA:%d:%d:0: VPL col: (%f|%f|%f|%f), VPL pos: (%f|%f|%f|%f)\n",
						depth, x,
						vpl_col[ray_index+offset].x, vpl_col[ray_index+offset].y, vpl_col[ray_index+offset].z, vpl_col[ray_index+offset].w,
						vpl_pos[ray_index+offset].x, vpl_pos[ray_index+offset].y, vpl_pos[ray_index+offset].z, vpl_pos[ray_index+offset].w);
					//printf("%d:%d:0: VPL col: (%f|%f|%f)\n", depth, x, vpl_col[ray_index+offset].x, vpl_col[ray_index+offset].y, vpl_col[ray_index+offset].z);

				return;
			}

			// write VPL data
			//vpl v(col, pos, normal, w_in, is);
			//int offset = vpl_store_offset[0];
			vpl_col[ray_index+offset] = make_float4(col.x/res.x, col.y/res.x, col.z/res.x, 0.f);
			vpl_pos[ray_index+offset] = make_float4(pos.x, pos.y, pos.z, 0.f);
			vpl_w_in[ray_index+offset] = w_in;
			vpl_is[ray_index+offset] = hit;
			
			if (x < 20)
			//if (x == 16)
				printf("CREA:%d:%d:1: VPL col: (%f|%f|%f), VPL pos: (%f|%f|%f)\n",
					depth, x,
					vpl_col[ray_index+offset].x, vpl_col[ray_index+offset].y, vpl_col[ray_index+offset].z,
					vpl_pos[ray_index+offset].x, vpl_pos[ray_index+offset].y, vpl_pos[ray_index+offset].z);
		}
	}

	void create_vpls::run() {
		int2 res = {light_rays->w, light_rays->h};
		k::create_vpls<<<launch_config>>>(res,
										  light_rays->rays.device_memory,
										  light_rays->intersections.device_memory,
										  light_throughput->data.device_memory,
										  vpl_store->col.device_memory,
										  vpl_store->pos.device_memory,
										  vpl_store->w_in.device_memory,
										  vpl_store->is.device_memory,
										  vpl_store_offset->data.device_memory,
										  depth,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->materials.device_memory);
	}

	namespace k {
		// Perceptional brightness of a color
		static __device__ inline float luma(const float3& rgb) {
			return dot({0.212671f, 0.715160f, 0.072169f}, rgb);
		}

		static __global__ void russian_roulette(int2 res, float4 *light_rays, tri_is *hits,
										   float3 *light_throughput, float3 *le,
										   uint4 *triangles, float4 *vert_norm, material *materials,
										   float *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			float4 light_ray_dir = light_rays[ray_index*2+1];
			// check if light ray is valid else continue
			if (light_ray_dir.x == 0 && light_ray_dir.y == 0 && light_ray_dir.z == 0)
				return;
			
			float3 throughput = light_throughput[ray_index]; 
			if (luma(throughput) == 0) {
				light_rays[ray_index*2+1] = {0,0,0,-1.f};
				return;
			}

			float xi = random[ray_index];
			float3 col = le[ray_index];
			//TODO-ML: check if this division is correct
			float q = luma({throughput.x/col.x, throughput.y/col.y, throughput.z/col.z});
			//TODO: is this q_j or q_j+1?
			// -> equals: float q = luma(v_j.col/Le_v_0);
			
			if (xi >= q) {
				light_rays[ray_index*2+1] = {0,0,0,-1.f};
				return;
			}

			light_throughput[ray_index] *= 1.0f/q;
		}
	}

	void russian_roulette::run() {
		int2 res = {light_rays->w, light_rays->h};
		rng.compute();
		k::russian_roulette<<<launch_config>>>(res,
										  light_rays->rays.device_memory,
										  light_rays->intersections.device_memory,
										  light_throughput->data.device_memory,
										  le->data.device_memory,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->materials.device_memory,
										  rng.random_numbers);
	}

	namespace k {
		static __global__ void sample_next_vpls(int2 res, float4 *light_rays, tri_is *hits,
										   float3 *light_throughput,
										   float4 *vpls_col, float4 *vpls_pos, float4 *vpls_w_in, tri_is *vpls_is,
										   int *vpl_store_offset, int depth,
										   uint4 *triangles, float4 *vert_norm, float2 *vertex_tc, material *materials,
										   float2 *random) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			float4 light_ray_dir = light_rays[ray_index*2+1];
			// check if light ray is valid else continue
			if (light_ray_dir.x == 0 && light_ray_dir.y == 0 && light_ray_dir.z == 0)
				return;

			int offset = depth*res.x;
			float4 vpl_col = vpls_col[ray_index+offset];
			float4 vpl_pos = vpls_pos[ray_index+offset];
			float4 vpl_w_in = vpls_w_in[ray_index+offset];
			tri_is vpl_is = vpls_is[ray_index+offset];

			float3 w_o { 0,0,0 };
			float3 org { 0,0,0 };
			float t_max = -FLT_MAX;
			float pdf = one_over_pi;
			float3 throughput { 0,0,0 };
			if (vpl_is.valid()) {
				uint4 vpl_tri = triangles[vpl_is.ref];
				material vpl_mat = materials[vpl_tri.w];

				float2 xi = random[(ray_index+1)%res.x];
				float3 sampled_dir = cosine_sample_hemisphere<float3>(xi);
				float3 vpl_ng  = hit_ng(vpl_is, vpl_tri, vert_norm);
				flip_normals_to_ray(vpl_ng, f3(vpl_w_in));
				w_o = align(sampled_dir, vpl_ng);
				org = f3(vpl_pos);
				t_max = FLT_MAX;

				pdf *= cdot(w_o, vpl_ng);
				float3 f = lambertian_reflection(w_o, -f3(vpl_w_in), vpl_ng, vpl_tri, vpl_is, vpl_mat, vertex_tc);

				// Setup the throughput for the next VPL
				float D = cdot(w_o, vpl_ng); //D_v_j(v_j+1)
				throughput = light_throughput[ray_index] * D*f/pdf; //throughput for v_j+1

				if (x < 20) {
					printf("scalar w_o: %f\n", dot(w_o, vpl_ng));
					printf("scalar w_in: %f\n", dot(-f3(vpl_w_in), vpl_ng));
					printf("normal(%f|%f|%f)\n", vpl_ng.x, vpl_ng.y, vpl_ng.z);
					printf("w_in(%f|%f|%f)\n", vpl_w_in.x, vpl_w_in.y, vpl_w_in.z);
					printf("w_out(%f|%f|%f)\n", w_o.x, w_o.y, w_o.z);
					printf("random: %f\n", random[ray_index]);
				}

				if (x < 20)
				//if (x == 16)
					printf("%d:%d: throughput: (%f|%f|%f)\tpdf: %f\tvalid: %d\tD: %f\tf: (%f|%f|%f)\n",
							depth, x,
							throughput.x, throughput.y, throughput.z,
							pdf, vpl_is.valid(), D,
							f.x, f.y, f.z);
			}
			light_rays[ray_index*2+0] = make_float4(org.x, org.y, org.z, 0.0001f);
			light_rays[ray_index*2+1] = make_float4(w_o.x, w_o.y, w_o.z, t_max);
			light_throughput[ray_index] = throughput;

			return;

			/*
			//int offset = vpl_store_offset[0];
			int offset = depth*res.x;
			//if (x == 0)
			//	printf("Offset: %d\n", offset);
			float4 vpl_col = vpls_col[ray_index+offset];
			float4 vpl_pos = vpls_pos[ray_index+offset];
			float4 vpl_w_in = vpls_w_in[ray_index+offset];
			tri_is vpl_is = vpls_is[ray_index+offset];

			uint4 vpl_tri = triangles[vpl_is.ref];
			float3 vpl_ng  = hit_ng(vpl_is, vpl_tri, vert_norm);
			material vpl_mat = materials[vpl_tri.w];

			// Sample ray to next VPL
			//diff_geom v_geometry(v.is, *pf->sd);
			float2 xi = random[ray_index];
			//auto [w_o, f, pdf] = v_geometry.mat->brdf->sample(v_geometry, -f3(vpl_w_in), xi); //f(v_j-1->v_j->v_j+1)
			// extracted from lambertian reflection
			//flip_normals_to_ray(vpl_ng, -f3(vpl_w_in));
			float3 w_o = align(cosine_sample_hemisphere<float3>(xi), vpl_ng);
			if (x < 20) {
				printf("scalar w_o: %f\n", dot(w_o, vpl_ng));
				printf("scalar w_in: %f\n", dot(-f3(vpl_w_in), vpl_ng));
				printf("normal(%f|%f|%f)\n", vpl_ng);
			}

			//flip_normals_to_ray(vpl_ng, w_o);
			//TODO-ML: handle this case
			//if (!same_hemisphere(w_o, vpl_ng))
			//	return { w_i, vec3(0), 0 };
			
			//TODO-ML: which method is correct now? :/
			//float pdf_f = absdot(vpl_ng, w_o) * one_over_pi; //p(w_j)
			float pdf_f = absdot(vpl_ng, f3(vpl_w_in)) * one_over_pi; //p(w_j)
			assert(std::isfinite(pdf_f));
			//return { w_i, f(geom, w_o, w_i), pdf_val };
			//float3 f = layered_gtr2(w_o, f3(vpl_w_in), vpl_ng, vpl_tri, vpl_is, vpl_mat, vertex_tc);
			float3 f = lambertian_reflection(w_o, f3(vpl_w_in), vpl_ng, vpl_tri, vpl_is, vpl_mat, vertex_tc);

			// Note: 'pdf_f' does not equal 'pdf' (returned from 'sample')
			//float pdf_f = v_geometry.mat->brdf->pdf(v_geometry, w_o, -v.w_in); //p(w_j)
			light_rays[ray_index*2+0] = make_float4(vpl_pos.x, vpl_pos.y, vpl_pos.z, 0);
			//light_rays[ray_index*2+0] = vpl_pos;
			float t_max = FLT_MAX; //TODO-ML: correct?
			light_rays[ray_index*2+1] = make_float4(w_o.x, w_o.y, w_o.z, t_max);

			// Setup the throughput for the next VPL
			float D = cdot(w_o, vpl_ng); //D_v_j(v_j+1)
			//TODO-ML: does this calculation work properly? (float3 * float3)
			light_throughput[ray_index] = light_throughput[ray_index] * D*f/pdf_f; //throughput for v_j+1

			if (x < 20)
			//if (x == 16) {
				printf("%d:%d: throughput: (%f|%f|%f)\tpdf: %f\tvalid: %d\tD: %f\tf: (%f|%f|%f)\n",
						depth, x,
						light_throughput[ray_index].x, light_throughput[ray_index].y, light_throughput[ray_index].z,
						pdf_f, vpl_is.valid(), D,
						f.x, f.y, f.z);
				/*printf("SAMP:%d:%d: pos(%f|%f|%f|%f), dir(%f|%f|%f|%f)\n",
						depth, x,
						light_rays[ray_index*2+0].x, light_rays[ray_index*2+0].y, light_rays[ray_index*2+0].z, light_rays[ray_index*2+0].w,
						light_rays[ray_index*2+1].x, light_rays[ray_index*2+1].y, light_rays[ray_index*2+1].z, light_rays[ray_index*2+1].w);
			*/
		}
	}

	void sample_next_vpls::run() {
		int2 res = {light_rays->w, light_rays->h};
		rng.compute();
		k::sample_next_vpls<<<launch_config>>>(res,
										  light_rays->rays.device_memory,
										  light_rays->intersections.device_memory,
										  light_throughput->data.device_memory,
										  vpl_store->col.device_memory,
										  vpl_store->pos.device_memory,
										  vpl_store->w_in.device_memory,
										  vpl_store->is.device_memory,
										  vpl_store_offset->data.device_memory,
										  depth,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->vertex_tc.device_memory,
										  pf->sd->materials.device_memory,
										  rng.random_numbers);
	}

	namespace k {
		struct valid_vpl_f {
			float compare;
			__host__ inline valid_vpl_f(float compare) : compare(compare) {}
			__host__ inline valid_vpl_f() : compare(0) {}
			
			__device__ inline bool operator()(const float &a) const {
				return a >= compare;
			}
		};
		struct valid_vpl_f3 {
			float3 compare;
			__host__ inline valid_vpl_f3(float3 compare) : compare(compare) {}
			
			__device__ inline bool operator()(const float3 &a) const {
				return (!(a.x == compare.x && a.y == compare.y && a.z == compare.z));
			}
		};
		struct valid_vpl_f4 {
			float compare;
			__host__ inline valid_vpl_f4(float compare) : compare(compare) {}
			__host__ inline valid_vpl_f4() : compare(0) {}
			
			__device__ inline bool operator()(const float4 &a) const {
				return a.w >= compare;
			}
		};
		struct valid_vpl_tri_is {
			float compare;
			__host__ inline valid_vpl_tri_is(float compare) : compare(compare) {}
			__host__ inline valid_vpl_tri_is() : compare(FLT_MAX) {}

			__device__ inline bool operator()(const tri_is &a) const {
				//return a.t != compare;
				return a.valid();
			}
		};

		static __global__ void copy_vpls(int2 res,
										   float4 *store_col, float4 *store_pos, float4 *store_w_in, tri_is *store_is,
										   float4 *vpls_col, float4 *vpls_pos, float4 *vpls_w_in, tri_is *vpls_is,
										   int *vpl_count,
										   uint4 *triangles, float4 *vert_norm, material *materials,
										   float *scale, int sppx) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			//cub::DeviceSelect::If()

			// testing area
			/*vpls_col[ray_index] = store_col[ray_index];
			vpls_pos[ray_index] = store_pos[ray_index];
			vpls_w_in[ray_index] = store_w_in[ray_index];
			vpls_is[ray_index] = store_is[ray_index];*/

			//TODO-ML: how do you do this generally?
			if (ray_index == 0) {
				//TODO-ML: calculate actual size
				//vpl_count[0] = res.x*res.y;
				printf("Count: %d\n", vpl_count[0]);

				//Calculate scaling factor
				float avg_path_len = vpl_count[0] * (1.f/res.x);
				//scale[0] = avg_path_len/vpls_per_sample;
				float vpls_per_sample = vpl_count[0] * (1.f/sppx);
				scale[0] = vpl_count[0] * (1.f/vpls_per_sample);

				//scale[0] = res.y * (1.f/vpls_per_sample);
				printf("AVG len: %f, scale: %f\n", avg_path_len, scale[0]);
				printf("Count: %d, VPLs p. samp.: %d\n", vpl_count[0], vpls_per_sample);
				printf("Count: %d, VPLs p. samp.: %f, AVG len: %f, scale: %f\n", vpl_count[0], vpls_per_sample, avg_path_len, scale[0]);

				float3 cur_normal = hit_ng(vpls_is[0], triangles[vpls_is[0].ref], vert_norm);
				flip_normals_to_ray(cur_normal, f3(vpls_w_in[0]));
				vpl_stats_device(store_is, store_w_in, vpl_count[0], triangles, vert_norm);
				//printf("cur_normal: %f|%f|%f\n", cur_normal.x, cur_normal.y, cur_normal.z);
				//printf("w_in: %f|%f|%f\n", store_w_in[0].x, store_w_in[0].y, store_w_in[0].z);
			}

			/*if (x < 20)
			//if (x == 16)
				printf("COPY:%d:0: VPL col: (%f|%f|%f|%f), VPL pos: (%f|%f|%f|%f)\n",
					x,
					store_col[ray_index].x, store_col[ray_index].y, store_col[ray_index].z, store_col[ray_index].w,
					store_pos[ray_index].x, store_pos[ray_index].y, store_pos[ray_index].z, store_pos[ray_index].w);*/

			/*if (x < 10) {
				printf("%d: throughput: (%f|%f|%f)\tpdf: %f\tvalid: %d\tD: %f\tf: (%f|%f|%f)\n",
						x,
						light_throughput[ray_index].x, light_throughput[ray_index].y, light_throughput[ray_index].z,
						pdf_f, vpl_is.valid(), D,
						f.x, f.y, f.z);
			}*/
		}
	}

	/*void copy_inplace(float3 *device_data, int data_size, int *device_selected_out, const float3 &compare_value, global_memory_buffer<char> &temp_storage) {
		size_t required_temp_storage_size;
		
		cub::DeviceSelect::If(nullptr, required_temp_storage_size, device_data, device_selected_out, data_size, k::valid_vpl_f3(compare_value));
		if (required_temp_storage_size > temp_storage.size) {
			temp_storage.resize(required_temp_storage_size);
		}

		CHECK_CUDA_ERROR(cub::DeviceSelect::If(temp_storage.device_memory, required_temp_storage_size, device_data, device_selected_out, data_size, k::valid_vpl_f3(compare_value)), "");
		CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
	}*/

	void copy_vpls::run() {
		global_memory_buffer<char> temp_memory("temp_mem", 0);
		// testing area
		/*std::vector<float3> data;
		global_memory_buffer<float3> device_data("test_select", 0);
		for (int i = 0; i < 10; i++)
			data.push_back(i%2?float3{1.f, 2.f, 3.f}:float3{0,0,0});

		device_data.upload(data);
		//global_memory_buffer<int> num_out("num_out", 1);
		
		vpl_count->data.download();
		std::cout << "Valid test: " << vpl_count->data.host_data[0] << std::endl;

		//copy_inplace(device_data.device_memory, device_data.size, vpl_count->data.device_memory, float3{0,0,0}, temp_memory);
		device_data.download();
		vpl_count->data.download();
		std::cout << "Valid test: " << vpl_count->data.host_data[0] << std::endl;
		for (int i = 0; i < vpl_count->data.host_data[0]; i++)
			std::cout << device_data.host_data[i].x << " " << device_data.host_data[i].y << " " << device_data.host_data[i].z << std::endl;*/
		// end testing area

		//TODO-ML: get store size as res...
		int2 res = {vpl_store->w, vpl_store->h};
		int data_size = res.x*res.y;
		size_t required_temp_memory_size;
		//int2 res = {vpl_store->w,1};
		//int2 res = {10,1};

		// only count valid vpls
		cub::DeviceSelect::If(nullptr, required_temp_memory_size, vpl_store->col.device_memory, vpls->col.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4());
		if (required_temp_memory_size > temp_memory.size)
			temp_memory.resize(required_temp_memory_size);
		CHECK_CUDA_ERROR(cub::DeviceSelect::If(temp_memory.device_memory, required_temp_memory_size, vpl_store->col.device_memory, vpls->col.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4()), "");
		CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		vpl_count->data.download();
		std::cout << "Valid vpls (by col): " << vpl_count->data.host_data[0] << std::endl;

		cub::DeviceSelect::If(nullptr, required_temp_memory_size, vpl_store->pos.device_memory, vpls->pos.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4());
		if (required_temp_memory_size > temp_memory.size)
			temp_memory.resize(required_temp_memory_size);
		CHECK_CUDA_ERROR(cub::DeviceSelect::If(temp_memory.device_memory, required_temp_memory_size, vpl_store->pos.device_memory, vpls->pos.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4()), "");
		CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		vpl_count->data.download();
		std::cout << "Valid vpls (by pos): " << vpl_count->data.host_data[0] << std::endl;

		vpls->col.download();
		vpls->pos.download();
		vpls->w_in.download();
		vpls->is.download();
		vpl_stats(vpls, vpl_count->data.host_data[0]);

		// Select valid vpls
		// col | TODO-ML: check if rex.x*res.y == vpls->col.size
		//copy_inplace(vpls->col.device_memory, res.x*res.y, num_out.device_memory, FLT_MAX, temp_memory);
		/*cub::DeviceSelect::If(nullptr, required_temp_memory_size, vpl_store->col.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4());
		if (required_temp_memory_size > temp_memory.size)
			temp_memory.resize(required_temp_memory_size);
		CHECK_CUDA_ERROR(cub::DeviceSelect::If(temp_memory.device_memory, required_temp_memory_size, vpl_store->col.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4()), "");
		CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		vpl_count->data.download();
		std::cout << "Valid col: " << vpl_count->data.host_data[0] << std::endl;

		// pos
		cub::DeviceSelect::If(nullptr, required_temp_memory_size, vpl_store->pos.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4());
		if (required_temp_memory_size > temp_memory.size)
			temp_memory.resize(required_temp_memory_size);
		CHECK_CUDA_ERROR(cub::DeviceSelect::If(temp_memory.device_memory, required_temp_memory_size, vpl_store->pos.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4()), "");
		CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		vpl_count->data.download();
		std::cout << "Valid pos: " << vpl_count->data.host_data[0] << std::endl;

		// w_in
		cub::DeviceSelect::If(nullptr, required_temp_memory_size, vpl_store->w_in.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4());
		if (required_temp_memory_size > temp_memory.size)
			temp_memory.resize(required_temp_memory_size);
		CHECK_CUDA_ERROR(cub::DeviceSelect::If(temp_memory.device_memory, required_temp_memory_size, vpl_store->w_in.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_f4()), "");
		CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		vpl_count->data.download();
		std::cout << "Valid w_in: " << vpl_count->data.host_data[0] << std::endl;

		// intersection
		cub::DeviceSelect::If(nullptr, required_temp_memory_size, vpl_store->is.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_tri_is());
		if (required_temp_memory_size > temp_memory.size)
			temp_memory.resize(required_temp_memory_size);
		CHECK_CUDA_ERROR(cub::DeviceSelect::If(temp_memory.device_memory, required_temp_memory_size, vpl_store->is.device_memory, vpl_count->data.device_memory, data_size, k::valid_vpl_tri_is()), "");
		CHECK_CUDA_ERROR(cudaDeviceSynchronize(), "");
		vpl_count->data.download();
		std::cout << "Valid is: " << vpl_count->data.host_data[0] << std::endl;*/

		k::copy_vpls<<<launch_config>>>(res,
										  vpl_store->col.device_memory,
										  vpl_store->pos.device_memory,
										  vpl_store->w_in.device_memory,
										  vpl_store->is.device_memory,
										  vpls->col.device_memory,
										  vpls->pos.device_memory,
										  vpls->w_in.device_memory,
										  vpls->is.device_memory,
										  vpl_count->data.device_memory,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->materials.device_memory,
										  scale->data.device_memory,
										  sppx);


		// debug
		bool export_debug_obj = false;
		if (export_debug_obj) {
			// begin writing paths.obj
			objdraw::obj_writer obj_writer("paths_cuda.obj");

			//for (auto& p : obj_paths)
			//	obj_writer.write_path(p);

			// draw path vertices in paths.obj as icospheres
			/*for (auto v_0 : obj_v_0_samples) {
				objdraw::icosphere sphere(v_0, 0.01f);
				obj_writer.write_icosphere(sphere);
			}*/
			//for (int i = 0; i < 10; i++) {
			for (int i = 0; i < res.x*res.y; i++) {
				if (vpls->is.host_data[i].t == FLT_MAX) continue;

				float4 v = vpls->pos.host_data[i];
				objdraw::icosphere sphere(vec3(v.x, v.y, v.z), 0.02f);
				obj_writer.write_icosphere(sphere);
			}
		}
	}

	/* integration */
	namespace k {
		static __global__ void sample_vpls(int2 res,
										   float4 *camrays, tri_is *hits, float4 *shadowrays, float4 *framebuffer,
										   uint4 *triangles, float4 *vert_norm, material *materials,
										   float4 *vpls_col, float4 *vpls_pos, float4 *vpls_w_in, tri_is *vpls_is,
										   float4 *sampled_vpls_col, float4 *sampled_vpls_pos, float4 *sampled_vpls_w_in, tri_is *sampled_vpls_is,
										   int *vpl_count,
										   int *current_sample, int vpls_per_sample, int vpl_offset) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			//float3 radiance {0,1.f,0};
			//framebuffer[ray_index] = framebuffer[ray_index] + make_float4(radiance.x, radiance.y, radiance.z, 1.0);

			tri_is hit = hits[ray_index];

			//int pos = random[ray_index] * vpl_count[0];
			//int pos = random[ray_index] * 1063;
			//TODO-ML: proper handling for this case
			//if (pos == vpl_count[0]) pos--;
			//pos = 110;
			//int pos = current_sample[0] * vpls_per_sample[0] + vpl_offset[0];
			int pos = current_sample[0] * vpls_per_sample + vpl_offset;
			float4 vpl_col = vpls_col[pos];
			float4 vpl_pos = vpls_pos[pos];
			float4 vpl_w_in = vpls_w_in[pos];
			tri_is vpl_is = vpls_is[pos];
			/*if (x == 10 && y == 20) {
				printf("%d: VPL col: (%f|%f|%f)\n", x, vpls_col[1061].x, vpls_col[1061].y, vpls_col[1061].z);
				printf("%d: VPL col: (%f|%f|%f)\n", x, vpls_col[1062].x, vpls_col[1062].y, vpls_col[1062].z);
				printf("%d: VPL col: (%f|%f|%f)\n", x, vpls_col[1063].x, vpls_col[1063].y, vpls_col[1063].z);
			}*/

			//auto [shadow_ray, col_delete, pdf_delete] = v.sample_Li(hit, vec2(0));
			//float3 to_light = pos - from.x;
			float3 from   = f3(camrays[ray_index*2+0]) + hit.t * f3(camrays[ray_index*2+1]);
			float3 target = f3(vpl_pos); //f3(bary_interpol(vert_pos[l_tri.x],  vert_pos[l_tri.y],  vert_pos[l_tri.z],  bc.x, bc.y));
			float3 to_light = target - from;
			
			float tmax = length(to_light);
			to_light /= tmax;
			tmax -= eps;
			float4 ray_org = make_float4(from.x, from.y, from.z, 0.0001);
			float4 ray_dir = make_float4(to_light.x, to_light.y, to_light.z, tmax);

			shadowrays[ray_index*2+0] = ray_org;
			shadowrays[ray_index*2+1] = ray_dir;

			sampled_vpls_col[ray_index] = vpl_col;
			sampled_vpls_pos[ray_index] = vpl_pos;
			sampled_vpls_w_in[ray_index] = vpl_w_in;
			sampled_vpls_is[ray_index] = vpl_is;

			// testing
			//framebuffer[ray_index] = make_float4(vpl_count[0], 0, 0, 1.0);

			/*if (x < 20)
			//if (x == 16)
				printf("ISMP:%d:0: VPL col: (%f|%f|%f|%f), VPL pos: (%f|%f|%f|%f)\n",
					x,
					vpls_col[ray_index].x, vpls_col[ray_index].y, vpls_col[ray_index].z, vpls_col[ray_index].w,
					vpls_pos[ray_index].x, vpls_pos[ray_index].y, vpls_pos[ray_index].z, vpls_pos[ray_index].w);*/

			//if (ray_index == 111)
			//	printf("ISMP: VPL index: %d\n", pos);
		}
	}

	void sample_vpls::run() {
		int2 res = frame_res();
		rng.compute();
		k::sample_vpls<<<launch_config>>>(res,
										  camrays->rays.device_memory,
										  camrays->intersections.device_memory,
										  shadowrays->rays.device_memory,
										  camrays->framebuffer.device_memory,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->materials.device_memory,
										  vpls->col.device_memory,
										  vpls->pos.device_memory,
										  vpls->w_in.device_memory,
										  vpls->is.device_memory,
										  sampled_vpls->col.device_memory,
										  sampled_vpls->pos.device_memory,
										  sampled_vpls->w_in.device_memory,
										  sampled_vpls->is.device_memory,
										  vpl_count->data.device_memory,
										  sample_index->data.device_memory,
										  vpls_per_sample,
										  vpl_offset);
	}

	namespace k {
		static __global__ void integrate_vpl_samples(int2 res,
										   float4 *camrays, tri_is *cam_hits,
										   float4 *shadowrays, tri_is *shadow_hits,
										   float4 *framebuffer,
										   uint4 *triangles, float4 *vert_norm, float2 *vertex_tc, material *materials,
										   float4 *sampled_vpls_col, float4 *sampled_vpls_pos, float4 *sampled_vpls_w_in, tri_is *sampled_vpls_is,
										   float *scale, int *current_sample, int vpls_per_sample, int vpl_offset,
										   int *cnt_integrated) {
			int x = threadIdx.x + blockIdx.x*blockDim.x;
			int y = threadIdx.y + blockIdx.y*blockDim.y;
			int ray_index = y*res.x + x;
			if (x >= res.x || y >= res.y)
				return;

			/*float3 radiance_test {0,1.f,0};
			framebuffer[ray_index] = framebuffer[ray_index] + make_float4(radiance_test.x, radiance_test.y, radiance_test.z, 1.0);
			return;*/
			
			/*if (x < 20)
			//if (x == 16)
				printf("SMP1:%d:0: VPL col: (%f|%f|%f|%f), VPL pos: (%f|%f|%f|%f)\n",
					x,
					sampled_vpls_col[ray_index].x, sampled_vpls_col[ray_index].y, sampled_vpls_col[ray_index].z, sampled_vpls_col[ray_index].w,
					sampled_vpls_pos[ray_index].x, sampled_vpls_pos[ray_index].y, sampled_vpls_pos[ray_index].z, sampled_vpls_pos[ray_index].w);*/

			tri_is hit = cam_hits[ray_index];
			tri_is shadow_hit = shadow_hits[ray_index];
			tri_is vpl_is = sampled_vpls_is[ray_index];
			float3 radiance {0,0,0};
			float4 shadowray_dir = shadowrays[2*ray_index+1];

			float3 test_normal {0,0,0};
			float3 test_entered {0,0,0};
			float3 test_ref {0,0,0};
			float3 test_is {0,0,0};
			//if (hit.valid() && shadowray_dir.w > 0 && !shadow_hit.valid()) {
			if (hit.valid() && !shadow_hit.valid() && vpl_is.valid()) {
					/*if (x < 20)
					printf("SMP2:%d:0: VPL col: (%f|%f|%f|%f), VPL pos: (%f|%f|%f|%f)\n",
						x,
						sampled_vpls_col[ray_index].x, sampled_vpls_col[ray_index].y, sampled_vpls_col[ray_index].z, sampled_vpls_col[ray_index].w,
						sampled_vpls_pos[ray_index].x, sampled_vpls_pos[ray_index].y, sampled_vpls_pos[ray_index].z, sampled_vpls_pos[ray_index].w);*/
				// VPL color
				float3 vpl_col = f3(sampled_vpls_col[ray_index]);
				
				//float t = length(v_geometry.x - hit.x);
				float t = shadowray_dir.w;

				// brdf at hit (x)
				float3 x_w_o = -f3(camrays[2*ray_index+1]);
				float3 x_w_i = f3(shadowray_dir);
				uint4  x_tri = triangles[hit.ref];
				float3 x_ng  = hit_ng(hit, x_tri, vert_norm);
				material x_mat = materials[x_tri.w];
				//float3 f_x = layered_gtr2(x_w_o, x_w_i, x_ng, x_tri, hit, x_mat, vertex_tc);
				float3 f_x = lambertian_reflection(x_w_o, x_w_i, x_ng, x_tri, hit, x_mat, vertex_tc);

				// brdf at vpl (v)
				float3 vpl_w_o = -f3(shadowray_dir);
				float3 vpl_w_i = -f3(sampled_vpls_w_in[ray_index]);
				uint4 vpl_tri = triangles[vpl_is.ref];
				float3 vpl_ng  = hit_ng(vpl_is, vpl_tri, vert_norm);
				//float3 vpl_ng  = {0,0,1.f};
				//flip_normals_to_ray(vpl_ng, f3(shadowray_dir));
				material vpl_mat = materials[vpl_tri.w];
				//float3 f_v = layered_gtr2(vpl_w_o, vpl_w_i, vpl_ng, vpl_tri, shadow_hit, vpl_mat, vertex_tc);
				float3 f_v = lambertian_reflection(vpl_w_o, vpl_w_i, vpl_ng, vpl_tri, shadow_hit, vpl_mat, vertex_tc);

				float D_x = cdot(x_ng, f3(shadowray_dir)); // D_x(v)
				float D_v = cdot(vpl_ng, -f3(shadowray_dir)); // D_v(x)
				float G = D_x*D_v/(t*t);
				G = G > 0.1f ? 0.1f : G; // sibenik
				//G = G > 1.f ? 1.f : G; // cornell

				radiance = f_x*G*vpl_col*f_v;

				//Scale to part of avg path length
				//TODO-ML: implement scaling...
				radiance *= scale[0];

				// testing
				//radiance = vpl_col*f_x*G*f_v*scale[0];
				//radiance = f_x*vpl_col*f_v;
				//radiance = {t,t,t};
				//radiance = vpl_w_i;
				//radiance = x_w_o;
				test_normal = vpl_ng;
				if (test_normal.x < 0) test_normal.x = test_normal.x * -1.f;
				if (test_normal.y < 0) test_normal.y = test_normal.y * -1.f;
				if (test_normal.z < 0) test_normal.z = test_normal.z * -1.f;
				test_entered = {0,0,1.f};
				//test_ref = {hit.ref*0.01f,hit.ref*0.01f,hit.ref*0.01f};
				test_ref = {vpl_is.ref*0.01f,vpl_is.ref*0.01f,vpl_is.ref*0.01f};
				test_is = {0,1,0};

				if (x == 153 && y == 230) {
					//printf("integrated VPL!\n");
					cnt_integrated[0]++;

					//printf("cur_normal: %f|%f|%f\n", vpl_ng.x, vpl_ng.y, vpl_ng.z);
				}
			}

			/*if (ray_index == 111)
				printf("R value before: %f\n", framebuffer[ray_index].x);
			// remove direct illumination for testing
			if (current_sample[0] == 0) {
				framebuffer[ray_index] = make_float4(0,0,0,1.f);
				if (ray_index == 111)
					printf("to 0!!!\n");
			}
			if (ray_index == 111)
				printf("R value after: %f\n", framebuffer[ray_index].x);*/

			/*if (x == 153 && y == 230) {
				framebuffer[ray_index] = make_float4(1.f,1.f,1.f,1.f);
				printf("Integrated VPLs: %d\n", cnt_integrated[0]);
				return;
			}*/

			framebuffer[ray_index] = framebuffer[ray_index] + make_float4(radiance.x, radiance.y, radiance.z, 0);
			//framebuffer[ray_index] = framebuffer[ray_index] + make_float4(test_normal.x, test_normal.y, test_normal.z, 0);
			//framebuffer[ray_index] = make_float4(radiance.x, radiance.y, radiance.z, 1.f);
			//framebuffer[ray_index] = make_float4(radiance.x, radiance.y, radiance.z, 0.125f);
			//framebuffer[ray_index] = make_float4(test_ref.x, test_ref.y, test_ref.z, 1.f);
			//framebuffer[ray_index] = make_float4(test_is.x, test_is.y, test_is.z, 1.f);
			//framebuffer[ray_index] = make_float4(test_normal.x, test_normal.y, test_normal.z, 1.f);
			//framebuffer[ray_index] = make_float4(test_entered.x, test_entered.y, test_entered.z, 1.f);
			//framebuffer[ray_index] = framebuffer[ray_index] + make_float4(test_entered.x, test_entered.y, test_entered.z, 0);

			if (ray_index == 0 && vpl_offset == vpls_per_sample-1) {
				current_sample[0]++;
				//printf("current_sample incremented: %d\n", current_sample[0]);
			}

			/*if (ray_index == 111)
				if (vpl_is.valid())
					printf("INTG: sampled\n");*/

			//if (x == 153 && y == 230)
			//	printf("Integrated VPLs: ", cnt_integrated[0]);
		}
	}

	void integrate_vpl_samples::run() {
		int2 res = frame_res();
		k::integrate_vpl_samples<<<launch_config>>>(res,
										  camrays->rays.device_memory,
										  camrays->intersections.device_memory,
										  shadowrays->rays.device_memory,
										  shadowrays->intersections.device_memory,
										  camrays->framebuffer.device_memory,
										  pf->sd->triangles.device_memory,
										  pf->sd->vertex_norm.device_memory,
										  pf->sd->vertex_tc.device_memory,
										  pf->sd->materials.device_memory,
										  sampled_vpls->col.device_memory,
										  sampled_vpls->pos.device_memory,
										  sampled_vpls->w_in.device_memory,
										  sampled_vpls->is.device_memory,
										  scale->data.device_memory,
										  sample_index->data.device_memory,
										  vpls_per_sample,
										  vpl_offset,
										  cnt_debug->data.device_memory);
	}

}