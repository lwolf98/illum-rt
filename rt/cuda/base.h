#pragma once

#include "libgi/wavefront-rt.h"
#include "rt/cpu/bvh.h"
#include "libgi/timer.h"
#include "libgi/subdivision.h"
#include "libgi/subdivision-helper.h"

#include "cuda-helpers.h"
//#include "cuda-operators.h"
//#include "optix-launch-params.h"

#include <cuda_runtime_api.h>
#define MULTIPROCESSOR_COUNT               82	// fixed (device dependent, 82 for RTX3090)
#define WARPSIZE                           32	// fixed (architecture-dependent)
#define MAX_WARPS_PER_MULTIPROCESSOR       48	// fixed (architecture-dependent)
#define DESIRED_WARPS_PER_BLOCK            6	// mostly arbitrary, doesn't really matter
#define DESIRED_THREADS_PER_BLOCK 		   DESIRED_WARPS_PER_BLOCK*32
#define DESIRED_BLOCKS_PER_MULTIPROCESSOR  MAX_WARPS_PER_MULTIPROCESSOR/DESIRED_WARPS_PER_BLOCK		// = 8
#define DESIRED_BLOCKS_COUNT               MULTIPROCESSOR_COUNT*DESIRED_BLOCKS_PER_MULTIPROCESSOR				// = 656
#define DESIRED_BLOCK_SIZE                 dim3(WARPSIZE, DESIRED_WARPS_PER_BLOCK, 1)
#define NUM_BLOCKS_FOR_RESOLUTION(resolution) dim3((resolution.x/DESIRED_BLOCK_SIZE.x) + 1, (resolution.y/DESIRED_BLOCK_SIZE.y) + 1, 1)

namespace wf {
	namespace cuda {
		
		//! \brief Take time of asynchronously running CUDA calls.
		struct timer : public wf::timer {
			std::map<std::string, std::pair<cudaEvent_t,cudaEvent_t>> events;
			void start(const std::string &name) override;
			void stop(const std::string &name) override;
			void synchronize() override;
		};

		// Stored as rows for efficient M*v
		struct mat3 {
			float a[9];

			static mat3 from(glm::mat3 mat) {
				return {{
					mat[0][0], mat[1][0], mat[2][0],
					mat[0][1], mat[1][1], mat[2][1],
					mat[0][2], mat[1][2], mat[2][2]
				}};
			}

			__device__ __forceinline__ float& at(int x, int y) {
				return a[y*3+x];
			}
			__device__ __forceinline__ float read_at(int x, int y) const {
				return a[y*3+x];
			}
			__device__ __forceinline__ float3 operator*(const float3 &f) const {
				float3 res;
				res.x = f.x * read_at(0,0) + f.y * read_at(1,0) + f.z * read_at(2,0);
				res.y = f.x * read_at(0,1) + f.y * read_at(1,1) + f.z * read_at(2,1);
				res.z = f.x * read_at(0,2) + f.y * read_at(1,2) + f.z * read_at(2,2);
				return res;
			}
			__device__ __forceinline__ float4 operator*(const float4 &f) const {
				float4 res;
				res.x = f.x * read_at(0,0) + f.y * read_at(1,0) + f.z * read_at(2,0);
				res.y = f.x * read_at(0,1) + f.y * read_at(1,1) + f.z * read_at(2,1);
				res.z = f.x * read_at(0,2) + f.y * read_at(1,2) + f.z * read_at(2,2);
				res.w = 1.0;
				return res;
			}
			// REVIEW: return order rows/cols correct?
			__device__ __forceinline__ mat3 transpose() const {
				return {{
					read_at(0, 0), read_at(0, 1), read_at(0, 2),
					read_at(1, 0), read_at(1, 1), read_at(1, 2),
					read_at(2, 0), read_at(2, 1), read_at(2, 2)
				}};
			}
			__device__ __forceinline__ mat3 inverse() const {
				// Prepare entries
				float a00 = read_at(0,0), a01 = read_at(1,0), a02 = read_at(2,0);
				float a10 = read_at(0,1), a11 = read_at(1,1), a12 = read_at(2,1);
				float a20 = read_at(0,2), a21 = read_at(1,2), a22 = read_at(2,2);

				// Compute cofactors
				float c00 =  a11 * a22 - a12 * a21;
				float c01 = -(a10 * a22 - a12 * a20);
				float c02 =  a10 * a21 - a11 * a20;

				float c10 = -(a01 * a22 - a02 * a21);
				float c11 =  a00 * a22 - a02 * a20;
				float c12 = -(a00 * a21 - a01 * a20);

				float c20 =  a01 * a12 - a02 * a11;
				float c21 = -(a00 * a12 - a02 * a10);
				float c22 =  a00 * a11 - a01 * a10;

				// Determinant
				float det = a00 * c00 + a01 * c01 + a02 * c02;
				float invDet = 1.0f / det; // TODO/REVIEW: handle det near 0 if necessary

				// Build inverse
				return {{
					c00 * invDet, c10 * invDet, c20 * invDet,
					c01 * invDet, c11 * invDet, c21 * invDet,
					c02 * invDet, c12 * invDet, c22 * invDet
				}};
			}
		};

		struct __align__(16) tri_is {
			float t;
			float beta;
			float gamma;
			subd::quad_ref subd_quad_ref;

			__device__ tri_is() : t(FLT_MAX), beta(-1), gamma(-1), prim_ref(0) {};
			__device__ tri_is(float t, float beta, float gamma, uint32_t ref) : t(t), beta(beta), gamma(gamma) {
				set_ref(ref);
			};
			__device__ __inline__ bool valid() const { return t != FLT_MAX; }

			__device__ __inline__ bool is_tri() const { return !(prim_ref & 1); }
			__device__ __inline__ uint32_t ref() const { return prim_ref >> 1; }
			__device__ __inline__ void set_ref(uint32_t ref, bool is_custom_prim = false) {
				prim_ref = is_custom_prim ?
							(ref << 1) | 1 :
							ref << 1;
			}

		private:
			uint32_t prim_ref;

		};

		struct __align__(16) simple_bvh_node /*: public node*/ {
			float3 box_l_min;
			float3 box_l_max;
			int link_l;
			float3 box_r_min;
			float3 box_r_max;
			int link_r;
			__host__ __device__ simple_bvh_node() {};
			__host__ simple_bvh_node(::aabb box_l, ::aabb box_r, int link_l, int link_r) : link_l(link_l), link_r(link_r) {
				box_l_min = {box_l.min.x, box_l.min.y, box_l.min.z};
				box_l_max = {box_l.max.x, box_l.max.y, box_l.max.z};
				box_r_min = {box_r.min.x, box_r.min.y, box_r.min.z};
				box_r_max = {box_r.max.x, box_r.max.y, box_r.max.z};
			};
			__host__ simple_bvh_node(binary_bvh_tracer<bbvh_triangle_layout::flat, bbvh_esc_mode::off>::node n) : simple_bvh_node(n.box_l, n.box_r, n.link_l, n.link_r) {};
			// __host__ simple_bvh_node(binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::off>::node n) : simple_bvh_node(n.box_l, n.box_r, n.link_l, n.link_r) {};
			// __host__ simple_bvh_node(binary_bvh_tracer<bbvh_triangle_layout::flat, bbvh_esc_mode::on>::node n) : simple_bvh_node(n.box_l, n.box_r, n.link_l, n.link_r) {};
			// __host__ simple_bvh_node(binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>::node n) : simple_bvh_node(n.box_l, n.box_r, n.link_l, n.link_r) {};
			__host__ __device__ __inline__ bool inner() const { return link_r >= 0; }
			__host__ __device__ __inline__ int32_t tri_offset() const { return -link_l; }
			__host__ __device__ __inline__ int32_t tri_count()  const { return -link_r; }
		};

		struct __align__(16) compact_bvh_node {
			/* Für innere Knoten gilt:
			 *    child_index < 0 => Child ist ein Blattknoten (mit dem Index -child_index)
			 * Für Blattknoten gilt:
			 *   child_index0 = -tri_offset
			 *   child_index1 = -tri_count
			 */
			float4 data1;	// (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)
			float4 data2;	// (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)
			float4 data3;	// (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)
			float4 data4;	// child_index0, child_index1
			__host__ __device__ compact_bvh_node() {};
		};

		struct compact_bvh_node_builder {
			static std::vector<compact_bvh_node> build(std::vector<binary_bvh_tracer<bbvh_triangle_layout::indexed, bbvh_esc_mode::on>::node> nodes);
		};

		struct buffer {
			std::string name;
			unsigned size = 0;	// number of elements! not bytes

			buffer(std::string name, unsigned size)
			: name(name), size(size) {
			}
			virtual ~buffer() {
			}
			virtual void print() {
			}
		};

		enum buffer_copy_mode_shallow { shallow_non_owning_copy };
		enum buffer_copy_mode_duplicate { mem_duplicating_copy_only }; // does not copy contents, but allocates new storage

		template<typename T> class global_memory_buffer : public buffer {
		protected:
			// those are protected/deleted to ensure buffers are explicitly duplicated or explicitly aliased
			global_memory_buffer(const global_memory_buffer &) = default;
			global_memory_buffer& operator=(const global_memory_buffer &) = delete;
		public:
			std::vector<T> host_data;
			T *device_memory = nullptr;
			bool owns_mem = true;

			global_memory_buffer(std::string name, unsigned size)
			: buffer(name, 0) {
				if (size > 0) resize(size);
			}

			global_memory_buffer(global_memory_buffer &org, buffer_copy_mode_shallow)
			: global_memory_buffer(org) {
				owns_mem = false;
			}

			global_memory_buffer(const global_memory_buffer &org, buffer_copy_mode_duplicate)
			: global_memory_buffer(org.name, org.size) {
			}

			global_memory_buffer(global_memory_buffer &&tmp) : buffer(std::move(tmp)), device_memory(tmp.device_memory), owns_mem(tmp.owns_mem), host_data(std::move(tmp.host_data)) {
				tmp.device_memory = nullptr;
			}
			global_memory_buffer& operator=(global_memory_buffer &&tmp) {
				name = std::move(tmp.name);
				size = tmp.size;
				device_memory = tmp.device_memory; tmp.device_memory = nullptr;
				owns_mem = tmp.owns_mem;
				host_data = std::move(tmp.host_data);
				return *this;
			}

			~global_memory_buffer() {
				if (device_memory && owns_mem) {
					CHECK_CUDA_ERROR(cudaFree(device_memory),name);
					CHECK_CUDA_ERROR(cudaDeviceSynchronize(),name);
				}
				device_memory = nullptr;
				size = 0;
			}
			
			explicit operator CUdeviceptr() {
				return (CUdeviceptr) device_memory;
			}

			int size_in_bytes() const {
				return size * sizeof(T);
			}

			void resize(int size) {
				assert(owns_mem);
				if (this->size == size) return;
				if (device_memory) {
					CHECK_CUDA_ERROR(cudaFree(device_memory), name);
					CHECK_CUDA_ERROR(cudaDeviceSynchronize(), name);
					device_memory = nullptr;
					this->size = 0;
				}

				T *new_device_memory = nullptr;
				CHECK_CUDA_ERROR(cudaMalloc((void**)&new_device_memory, size*sizeof(T)), name);
				CHECK_CUDA_ERROR(cudaDeviceSynchronize(), name);
				this->device_memory = new_device_memory;
				this->size = size;
			}

			void upload(const std::vector<T> &data) {
				upload(data.size(), data.data());
			}

			void upload(int size, const T *data) {
				resize(size);
				host_data.resize(size);

				std::copy(data, data + size, host_data.begin());

				CHECK_CUDA_ERROR(cudaMemcpy(device_memory, host_data.data(), size*sizeof(T), cudaMemcpyHostToDevice), name);
				CHECK_CUDA_ERROR(cudaGetLastError(), name);
				CHECK_CUDA_ERROR(cudaDeviceSynchronize(), name);
			}
			void download() {
				time_this_block(download_membuffer);
				if (host_data.size() != size)
					host_data.resize(size);

				CHECK_CUDA_ERROR(cudaMemcpy(host_data.data(), device_memory, size*sizeof(T), cudaMemcpyDeviceToHost), name);
				CHECK_CUDA_ERROR(cudaDeviceSynchronize(), name);
			}
			void free_host_data() {
				std::vector<T>{}.swap(host_data);
			}
		};

		template<typename T> class texture_buffer : public global_memory_buffer<T> {
		protected:
			texture_buffer(const texture_buffer &) = default;
			texture_buffer& operator=(const texture_buffer &) = delete;
		public:
			cudaTextureObject_t tex = 0;

			texture_buffer(std::string name, unsigned size)
			: global_memory_buffer<T>(name, size) {
				if (size > 0)
					update_texture();
			}
			
			texture_buffer(texture_buffer &org, buffer_copy_mode_shallow) : texture_buffer(org) {
				this->owns_mem = false;
			}
			
			texture_buffer(const texture_buffer &org, buffer_copy_mode_duplicate) : texture_buffer(org.name, org.size) {
			}

			texture_buffer(texture_buffer &&tmp) : global_memory_buffer<T>(tmp), tex(tmp.tex) {
				tmp.tex = 0;
			}

			texture_buffer& operator=(texture_buffer &&tmp) {
				global_memory_buffer<T>::operator=(std::move(tmp));
				tex = tmp.tex; tmp.tex = 0;
				return *this;
			}

			~texture_buffer() {
				if (tex != 0 && this->owns_mem)
					CHECK_CUDA_ERROR(cudaDestroyTextureObject(tex),this->name);
			}

			void update_texture() {
				assert(this->owns_mem);
				if (tex>0)
					CHECK_CUDA_ERROR(cudaDestroyTextureObject(tex),this->name);
				
				cudaResourceDesc res_desc = {};
				res_desc.resType = cudaResourceTypeLinear;
				res_desc.res.linear.devPtr = this->device_memory;
				res_desc.res.linear.sizeInBytes = this->size*sizeof(T);
				res_desc.res.linear.desc = cudaCreateChannelDesc<float4>();
				cudaTextureDesc tex_desc = {};
				memset(&tex_desc, 0, sizeof(tex_desc));
				tex_desc.addressMode[0] = cudaAddressModeClamp; // Wrap?
				tex_desc.addressMode[1] = cudaAddressModeClamp;
				tex_desc.addressMode[2] = cudaAddressModeClamp;
				tex_desc.filterMode = cudaFilterModePoint;
				tex_desc.readMode = cudaReadModeElementType;
				tex_desc.normalizedCoords = 0;

				CHECK_CUDA_ERROR(cudaCreateTextureObject(&tex, &res_desc, &tex_desc, nullptr),this->name);
			}

			void resize(int size) {
				global_memory_buffer<T>::resize(size);
				update_texture();
			}

			void upload(const std::vector<T> &data) {
				upload(data.size(), data.data());
			}

			void upload(int size, const T *data) {
				global_memory_buffer<T>::upload(size, data);
				update_texture();
			}

			/*void download() {
				global_memory_buffer<T>::download();
			}*/
		};

		struct texture_image {
			std::string name;
			cudaArray *underlying = nullptr;
			cudaTextureObject_t tex = 0;
			int w, h;
			int pitch;

			texture_image(const texture2d<vec3> &base)
			: name(base.name + "-on-cuda"), w(base.w), h(base.h) {
				float4 *src = new float4[w*h];
				#pragma omp parallel for
				for (int y = 0; y < h; ++y)
					for (int x = 0; x < w; ++x)
						src[y*w+x] = float4{ base.texel[y*w+x].x, base.texel[y*w+x].y, base.texel[y*w+x].z, 0 };
				
				create_cuda_resource(src);
				delete [] src;
			}

			texture_image(const texture2d<vec4> &base)
			: name(base.name + "-on-cuda"), w(base.w), h(base.h) {
				float4 *src = new float4[w*h];
				#pragma omp parallel for
				for (int y = 0; y < h; ++y)
					for (int x = 0; x < w; ++x)
						src[y*w+x] = float4{ base.texel[y*w+x].x, base.texel[y*w+x].y, base.texel[y*w+x].z, base.texel[y*w+x].w };
				
				create_cuda_resource(src);
				delete [] src;
			}

			private:
				void create_cuda_resource(const float4 *src) {
					auto chan_desc = cudaCreateChannelDesc(32, 32, 32, 32, cudaChannelFormatKindFloat);
					CHECK_CUDA_ERROR(cudaMallocArray(&underlying, &chan_desc, w, h), name);
					int spitch = w * sizeof(float4);
					CHECK_CUDA_ERROR(cudaMemcpy2DToArray(underlying, 0, 0, src, spitch, w*sizeof(float4), h, cudaMemcpyHostToDevice), name);
					cudaResourceDesc res_desc;
					memset(&res_desc, 0, sizeof(res_desc));
					res_desc.resType = cudaResourceTypeArray;
					res_desc.res.array.array = underlying;

					cudaTextureDesc tex_desc;
					memset(&tex_desc, 0, sizeof(tex_desc));
					tex_desc.addressMode[0] = cudaAddressModeWrap;
					tex_desc.addressMode[1] = cudaAddressModeWrap;
					tex_desc.filterMode = cudaFilterModeLinear;
					tex_desc.readMode = cudaReadModeElementType;
					tex_desc.normalizedCoords = 1;

					CHECK_CUDA_ERROR(cudaCreateTextureObject(&tex, &res_desc, &tex_desc, nullptr), name);
				}
			// TODO cleanup missing
		};

		struct raydata : public wf::raydata {
			std::string name;
			int w, h;
			texture_buffer<float4> rays;
			global_memory_buffer<tri_is> intersections;
			global_memory_buffer<float4> framebuffer;

			raydata(glm::ivec2 dim) : raydata(dim.x, dim.y) {}
			raydata(int w, int h) : w(w), h(h),
									rays("rays", 2*w*h),
									intersections("intersections", w*h),
									framebuffer("framebuffer", w*h)	{
				  rc->call_at_resolution_change[this] = [this](int w, int h) {
					  this->w = w;
					  this->h = h;
					  this->rays.resize(2*w*h);
					  this->intersections.resize(w*h);
					  this->framebuffer.resize(w*h);
				  };
			}
			~raydata() {
				rc->call_at_resolution_change.erase(this);
			}
		};

		struct material {
			float4 albedo;
			float4 emissive;
			cudaTextureObject_t albedo_tex;
			float ior, roughness;
		};

		struct patch_node {
			// memory layout and order of the following fields is important!
			float4 min_1;
			float4 min_2;
			float4 min_3;
			float4 max_1;
			float4 max_2;
			float4 max_3;

			void set_min(uint32_t index, const vec3 &v) {
				assert(index <= 3);
				if (index == 0)			{ min_1.x = v.x; min_1.y = v.y; min_1.z = v.z; }
				else if (index == 1)	{ min_1.w = v.x; min_2.x = v.y; min_2.y = v.z; }
				else if (index == 2)	{ min_2.z = v.x; min_2.w = v.y; min_3.x = v.z; }
				else					{ min_3.y = v.x; min_3.z = v.y; min_3.w = v.z; }
			}

			void set_max(uint32_t index, const vec3 &v) {
				assert(index <= 3);
				if (index == 0)			{ max_1.x = v.x; max_1.y = v.y; max_1.z = v.z; }
				else if (index == 1)	{ max_1.w = v.x; max_2.x = v.y; max_2.y = v.z; }
				else if (index == 2)	{ max_2.z = v.x; max_2.w = v.y; max_3.x = v.z; }
				else					{ max_3.y = v.x; max_3.z = v.y; max_3.w = v.z; }
			}

			float3 __device__ __forceinline__ get_min(uint32_t index) const {
				assert(index <= 3);
				if (index == 0)			return { .x = min_1.x, .y = min_1.y, .z = min_1.z };
				else if (index == 1)	return { .x = min_1.w, .y = min_2.x, .z = min_2.y };
				else if (index == 2)	return { .x = min_2.z, .y = min_2.w, .z = min_3.x };
				else					return { .x = min_3.y, .y = min_3.z, .z = min_3.w };
			}

			float3 __device__ __forceinline__ get_max(uint32_t index) const {
				assert(index <= 3);
				if (index == 0)			return { .x = max_1.x, .y = max_1.y, .z = max_1.z };
				else if (index == 1)	return { .x = max_1.w, .y = max_2.x, .z = max_2.y };
				else if (index == 2)	return { .x = max_2.z, .y = max_2.w, .z = max_3.x };
				else					return { .x = max_3.y, .y = max_3.z, .z = max_3.w };
			}

			//TODO: is it ok to use these (dynamic) variants?
			/*float3 __device__ __forceinline__ get_min(uint32_t index) const {
				assert(index <= 3);
				uint32_t off = index * 3;
				float *min_base = (float *)&min_1;
				return float3 { .x = min_base[off], .y = min_base[off+1], .z = min_base[off+2] };
			}

			float3 __device__ __forceinline__ get_max(uint32_t index) const {
				assert(index <= 3);
				uint32_t off = index * 3;
				float *max_base = (float *)&max_1;
				return float3 { .x = max_base[off], .y = max_base[off+1], .z = max_base[off+2] };
			}*/
		};

		int32_t __forceinline__ __device__ geometric_series4(int iterations) {
			return (1 - (1 << ((iterations+1)<<1))) / (-3);
		}

		struct subd_subpatch {
			uint32_t vert_start;
			mat3 trafo;
#ifdef PROJECTION
			mat3 proj;
#endif
			uint32_t bvh_node_offset;
			uint32_t subd_level;
			uint32_t parent_id; // TODO/TMP: Can probably be deleted and calculated in intersect
#ifdef BOX_APPROXIMATION
			float4 root_min; // TODO/REVIEW: float4 here good? or other variants more efficient?
			float4 root_max;
#endif

			__forceinline__ __device__ uint32_t len() const {
				return (1 << subd_level)+1;
			}
#ifdef BOX_APPROXIMATION
			__forceinline__ __device__ const void box_from_index(uint32_t index, const patch_node *nodes, float3 &box_min, float3 &box_max) const {
				if (subd_level == 0) {
					box_min = make_float3(root_min.x, root_min.y, root_min.z);
					box_max = make_float3(root_max.x, root_max.y, root_max.z);
					return;
				}

				nodes = &nodes[bvh_node_offset]; // REVIEW: nodes += bvh_node_offset;
				uint32_t modulo_mask = ~(0xFFFFFFFF << 2*subd_level);
				uint32_t quad_ref_local = index & modulo_mask;
				uint32_t node_index = (quad_ref_local >> 2) + geometric_series4(subd_level-2);
				uint32_t box_index = quad_ref_local & 0x3;
				box_min = nodes[node_index].get_min(box_index); // TODO/REVIEW: does this double copy the box? how to be more efficient?
				box_max = nodes[node_index].get_max(box_index);
			}
#endif
#ifdef PROJECTION
			float3 __forceinline__ __device__ oriented_to_projected(const float3 &p) const {
				return project(p, proj);
			}

			float3 __forceinline__ __device__ projected_to_oriented(const float3 &p) const {
				return project(p, proj.inverse());
			}
		private:
			static __forceinline__ __device__ float3 project(const float3 &a, const mat3 &P) {
				float3 tmp = P * make_float3(a.x, a.z, 1.f);
				return make_float3(tmp.x/tmp.z, a.y, tmp.y/tmp.z);
			}
#endif
		};

		struct subd_patch {
			uint32_t start_index;
			uint32_t material_id;
			uint32_t subd_level;
#ifdef BOX_APPROXIMATION
			float2 box_tcs[4];
			float4 box_norms[4]; // REVIEW: float4 here better than float3?
			uint32_t subpatch_offset;
#endif

			__forceinline__ __device__ uint32_t len() const {
				return (1 << subd_level)+1;
			}

			__forceinline__ __device__ uint4 subd_tri(int vert_quad_id, bool upper) const {
				uint4 tri;

				tri.w = material_id;
				if (upper) {
					tri.x = start_index + vert_quad_id;
					tri.y = start_index + vert_quad_id + len(); // vert down
					tri.z = start_index + vert_quad_id + 1; // vert right
				}
				else {
					// [FEAT-APPROX] update vertex order for box approx
					tri.x = start_index + vert_quad_id + len() + 1; // vert down right
					tri.y = start_index + vert_quad_id + 1; // vert right
					tri.z = start_index + vert_quad_id + len(); // vert down
				}

				return tri;
			}

			uint32_t __forceinline__ __device__ quad_ref_from_index(uint32_t index) const {
				uint2 xy = xy_from_index(index);
				return xy.y*len() + xy.x;
			}

#ifdef BOX_APPROXIMATION

			uint32_t __forceinline__ __device__ index_from_quad_ref(uint32_t vert_quad_id) const {
				uint32_t x = vert_quad_id % len(); //TODO/REVIEW: switch / and % to shift operations
				uint32_t y = vert_quad_id / len();
				return encode_morton(x, y);
			}

			__forceinline__ __device__ const subd_subpatch &subpatch_from_index(uint32_t index, const subd_subpatch *subpatches) const {
				//assert(subpatches.size() > 0);
				subpatches = &subpatches[subpatch_offset];
				uint32_t aligned_subd_level = subpatches[0].subd_level;
				uint32_t subpatch_id = index >> 2*aligned_subd_level; // divide by subpatch size (#quads in subpatch)
				return subpatches[subpatch_id];
			}

			float2 __forceinline__ __device__ global_uvs(subd::quad_ref quad_ref, float local_u, float local_v) const {
				uint32_t quad_len = len() - 1;
				auto [x, y] = xy_from_index(quad_ref.ref());
				float global_u = x * 1.f/quad_len;
				float global_v = y * 1.f/quad_len;

				float step = 1.f / quad_len;
				if (quad_ref.is_upper_tri()) {
					global_u += step * local_u;
					global_v += step * local_v;
				}
				else {
					global_u += step * (1.f - local_u);
					global_v += step * (1.f - local_v);
				}

				return make_float2(global_u, global_v);
			}
#endif

		private:
			uint2 __forceinline__ __device__ xy_from_index(uint32_t index) const {
				return make_uint2(
					decode_morton(index),		// x
					decode_morton(index >> 1)	// y
				);
			}

			//TODO: remove one of the variables (x, morton)
			static uint32_t __forceinline__ __device__ decode_morton(uint32_t morton) {
				uint32_t x = morton & 0x55555555;
				x = (x | (x >> 1)) & 0x33333333;
				x = (x | (x >> 2)) & 0x0F0F0F0F;
				x = (x | (x >> 4)) & 0x00FF00FF;
				x = (x | (x >> 8)) & 0x0000FFFF;
				return x;
			}

#ifdef BOX_APPROXIMATION
			static uint32_t __forceinline__ __device__ encode_morton(uint32_t x, uint32_t y) {
				auto spread_bits = [](uint32_t v) {
					v &= 0x0000FFFF; // clear upper bits
					v = (v | (v << 8)) & 0x00FF00FF;
					v = (v | (v << 4)) & 0x0F0F0F0F;
					v = (v | (v << 2)) & 0x33333333;
					v = (v | (v << 1)) & 0x55555555;
					return v;
				};

				return spread_bits(x) | (spread_bits(y) << 1);
			}
#endif
		};

		struct scene_refs {
			float4 *vertex_pos;
			float4 *vertex_norm;
			float2 *vertex_tc;
			uint4 *triangles;
			material *materials;
			texture_image *tex_images;

			subd_patch *patches;
#if !defined(BOX_APPROXIMATION) || defined(KEEP_GEOMETRY)
			float4 *patch_vertex_pos;
			float4 *patch_vertex_norm;
			float2 *patch_vertex_tc;
#endif

			subd_subpatch *subpatches;
			patch_node *patch_nodes;
		};

		struct scenedata {
			int n_vertices = 0, n_triangles = 0;
			texture_buffer<float4> vertex_pos;
			texture_buffer<float4> vertex_norm;
			texture_buffer<float2> vertex_tc;
			texture_buffer<uint4> triangles;
			global_memory_buffer<material> materials;
			std::vector<texture_image> tex_images;

			global_memory_buffer<subd_patch> patches;
			global_memory_buffer<subd_subpatch> subpatches;
			global_memory_buffer<patch_node> patch_nodes;
			global_memory_buffer<aabb> patch_root_boxes;
#if !defined(BOX_APPROXIMATION) || defined(KEEP_GEOMETRY)
			texture_buffer<float4> patch_vertex_pos;
			texture_buffer<float4> patch_vertex_norm;
			texture_buffer<float2> patch_vertex_tc;
#endif

			global_memory_buffer<scene_refs> refs;

			scenedata() : vertex_pos("vertex_pos", 0),
						  vertex_norm("vertex_norm", 0),
						  vertex_tc("vertex_tc", 0),
						  triangles("triangles", 0),
						  materials("materials", 0),
						  patches("patches", 0),
						  subpatches("subpatches", 0),
						  patch_nodes("patch_nodes", 0),
						  patch_root_boxes("patch_root_boxes", 0),
#if !defined(BOX_APPROXIMATION) || defined(KEEP_GEOMETRY)
						  patch_vertex_pos("patch_vertex_pos", 0),
						  patch_vertex_norm("patch_vertex_norm", 0),
						  patch_vertex_tc("patch_vertex_tc", 0),
#endif
						  refs("scene_refs", 0) {
			};
			scenedata(const scenedata &) = delete;
			scenedata(scenedata *org, buffer_copy_mode_shallow m) : vertex_pos(org->vertex_pos, m),
			                                                        vertex_norm(org->vertex_norm, m),
			                                                        vertex_tc(org->vertex_tc, m),
																	triangles(org->triangles, m),
																	materials(org->materials, m),
																	// tex_images not copied
																	n_vertices(org->n_vertices),
																	n_triangles(org->n_triangles),
																	patches(org->patches, m),
																	subpatches(org->subpatches, m),
																	patch_nodes(org->patch_nodes, m),
																	patch_root_boxes(org->patch_root_boxes, m),
#if !defined(BOX_APPROXIMATION) || defined(KEEP_GEOMETRY)
																	patch_vertex_pos(org->patch_vertex_pos, m),
																	patch_vertex_norm(org->patch_vertex_norm, m),
																	patch_vertex_tc(org->patch_vertex_tc, m),
#endif
																	refs(org->refs, m) {
				this->org = org;
			}
			void upload(scene *scene);
			scenedata *org = nullptr;
		};

		struct cpu_bvh_builder_cuda_scene_traits {
			scenedata *s;
			typedef uint4 tri_t;
			int  triangles() { return s->triangles.host_data.size(); }
			tri_t triangle(int index) { return s->triangles.host_data[index]; }
			int triangle_a(int index) { return s->triangles.host_data[index].x; }
			int triangle_b(int index) { return s->triangles.host_data[index].y; }
			int triangle_c(int index) { return s->triangles.host_data[index].z; }
			glm::vec3 vertex_pos(int index) { float4 v = s->vertex_pos.host_data[index]; return glm::vec3(v.x,v.y,v.z); }
			void replace_triangles(std::vector<tri_t> &&new_tris) {
				s->triangles.host_data = new_tris;
			}
		};

		struct batch_rt : public batch_ray_tracer {
			raydata *rd = nullptr;
			bool use_incoherence = false;
			float incoherence_r1 = 0; // TODO
			float incoherence_r2 = 0;

			int bvh_max_tris_per_node = 4;
			std::string bvh_type = "sah";

			texture_buffer<wf::cuda::compact_bvh_node> bvh_nodes;
			texture_buffer<uint> bvh_index;

			batch_rt() : bvh_nodes("bvh_nodes", 0), bvh_index("index", 0) {
			}
			virtual void build(scenedata *scene);
			void use(wf::raydata *rays) override { 
			    rd = dynamic_cast<raydata*>(rays);
			}
			bool interprete(const std::string &command, std::istringstream &in) override;
			void compute_closest_hit() override {
				compute_hit(false);
			}
			void compute_any_hit() override {
				compute_hit(true);
			}
			virtual void compute_hit(bool anyhit) = 0;
		};

		struct debug_info {
			int2 px_index;
		};

	}
}
