#include "libgi/subdivision.h"
#include "cuda-helpers.h"

namespace wf {
	namespace cuda {
		struct aabb_f3 {
			float3 min;
			float3 max;
		};

#ifdef QUANTIZATION
		static char quantize_xz(float val) {
			static float thresholds[] = { -1.f, -.2f, -.01f, -.001f, .001f, .01f, .2f, 1.f };
			char res = 0;

			return res;
		}
		static char quantize_y(float val) {

		}
		static float dequantize_xz(char val) {
			static float thresholds[] = { -1.f, -.2f, -.01f, -.001f, .001f, .01f, .2f, 1.f };
			float res = thresholds[2];

			return res;
		}
		static float dequantize_y(char val) {
			return 0.f;
		}

		void __device__ __forceinline__ set_012(char &field, char val) { field = (field & 0b00011111 | val << 5); }
		void __device__ __forceinline__ set_345(char &field, char val) { field = (field & 0b11100011 | val << 2); }
		void __device__ __forceinline__ set_67 (char &field, char val) { field = (field & 0b11111100 | val); }

		char __device__ __forceinline__ get_012(const char &field) { return ((field & 0b11100000) >> 5); }
		char __device__ __forceinline__ get_345(const char &field) { return ((field & 0b00011100) >> 2); }
		char __device__ __forceinline__ get_67 (const char &field) { return (field & 0b00000011); }
    
    #ifndef Y_SLAB_COMPRESSION
		void __device__ __forceinline__ set_01 (char &field, char val) { field = (field & 0b00111111 | val << 6); }
		void __device__ __forceinline__ set_23 (char &field, char val) { field = (field & 0b11001111 | val << 4); }
		void __device__ __forceinline__ set_45 (char &field, char val) { field = (field & 0b11110011 | val << 2); }
		//void __device__ __forceinline__ set_67 (char &field, char val) { field = (field & 0b11111100 | val); }

		char __device__ __forceinline__ get_01 (const char &field) { return ((field & 0b11000000) >> 6); }
		char __device__ __forceinline__ get_23 (const char &field) { return ((field & 0b00110000) >> 4); }
		char __device__ __forceinline__ get_45 (const char &field) { return (field & 0b00001100 >> 2); }
		//char __device__ __forceinline__ get_67 (char &field) { return (field & 0b00000011); }
    #endif
#endif

#ifndef SLAB_COMPRESSION
		struct patch_node {
	#ifndef QUANTIZATION
			// memory layout and order of the following fields is important!
			float4 min_1;
			float4 min_2;
			float4 min_3;
			float4 max_1;
			float4 max_2;
			float4 max_3;
			//-> 24 floats, 96 byte, 768 bit
			//non optimized layout:
			//-> 32 floats, 128 byte, 1024 bit
	#else
			/*
				Bit:	[  0|  1|  2|  3|  4|  5|  6|  7]
				b0xz	[0_x_min	|0_x_max	|0_z_min]
				b0yz	[0_y_min	|0_y_max	|0_z_max]
				b1xz	[1_x_min	|1_x_max	|1_z_min]
				b1yz	[1_y_min	|1_y_max	|1_z_max]
				b2xz	[2_x_min	|2_x_max	|2_z_min]
				b2yz	[2_y_min	|2_y_max	|2_z_max]
				b3xz	[3_x_min	|3_x_max	|3_z_min]
				b3yz	[3_y_min	|3_y_max	|3_z_max]
				-> 8 chars, 8 byte, 64 bit
			*/
			char box_data[8];
	#endif

			void set_min(uint32_t index, const vec3 &v) {
				//assert(index <= 3);
				if (index == 0)			{ min_1.x = v.x; min_1.y = v.y; min_1.z = v.z; }
				else if (index == 1)	{ min_1.w = v.x; min_2.x = v.y; min_2.y = v.z; }
				else if (index == 2)	{ min_2.z = v.x; min_2.w = v.y; min_3.x = v.z; }
				else					{ min_3.y = v.x; min_3.z = v.y; min_3.w = v.z; }
			}

			void set_max(uint32_t index, const vec3 &v) {
				//assert(index <= 3);
				if (index == 0)			{ max_1.x = v.x; max_1.y = v.y; max_1.z = v.z; }
				else if (index == 1)	{ max_1.w = v.x; max_2.x = v.y; max_2.y = v.z; }
				else if (index == 2)	{ max_2.z = v.x; max_2.w = v.y; max_3.x = v.z; }
				else					{ max_3.y = v.x; max_3.z = v.y; max_3.w = v.z; }
			}

			float3 __device__ __forceinline__ get_min(uint32_t index) const {
				//assert(index <= 3);
				if (index == 0)			return { .x = min_1.x, .y = min_1.y, .z = min_1.z };
				else if (index == 1)	return { .x = min_1.w, .y = min_2.x, .z = min_2.y };
				else if (index == 2)	return { .x = min_2.z, .y = min_2.w, .z = min_3.x };
				else					return { .x = min_3.y, .y = min_3.z, .z = min_3.w };
			}

			float3 __device__ __forceinline__ get_max(uint32_t index) const {
				//assert(index <= 3);
				if (index == 0)			return { .x = max_1.x, .y = max_1.y, .z = max_1.z };
				else if (index == 1)	return { .x = max_1.w, .y = max_2.x, .z = max_2.y };
				else if (index == 2)	return { .x = max_2.z, .y = max_2.w, .z = max_3.x };
				else					return { .x = max_3.y, .y = max_3.z, .z = max_3.w };
			}

			//TODO: is it ok to use these (dynamic) variants?
			//float3 __device__ __forceinline__ get_min(uint32_t index) const {
			//	assert(index <= 3);
			//	uint32_t off = index * 3;
			//	float *min_base = (float *)&min_1;
			//	return float3 { .x = min_base[off], .y = min_base[off+1], .z = min_base[off+2] };
			//}

			//float3 __device__ __forceinline__ get_max(uint32_t index) const {
			//	assert(index <= 3);
			//	uint32_t off = index * 3;
			//	float *max_base = (float *)&max_1;
			//	return float3 { .x = max_base[off], .y = max_base[off+1], .z = max_base[off+2] };
			//}
		};
#endif

#ifdef SLAB_COMPRESSION
	#ifndef HALF_SLAB_COMPRESSION
		#ifndef QUANTIZATION
		struct __align__(16) patch_node {
			// With Y-Slab Compression:
			// 8 slabs for xz -> 8 floats + 2 y coords -> 2 floats = 10 floats
			// Without Y-Slab Compression:
			// 8 slabs for xz -> 8 floats + 8 y coords -> 8 floats = 16 floats
			//
			// instead of: 6 floats (24 byte)
			//
			// slab x_0 x_1 x_2 x_3
			// slab z_0 z_1 z_2 z_3
			//          - MIN -                       - MAX -
			// box 0 -> x_0,z_0 | x_1,z_0 | x_0,z_1 | x_1,z_1
			// box 1 -> x_2,z_0 | x_3,z_0 | x_2,z_1 | x_3,z_1
			// box 2 -> x_0,z_2 | x_1,z_2 | x_0,z_3 | x_1,z_3
			// box 3 -> x_2,z_2 | x_3,z_2 | x_2,z_3 | x_3,z_3

			float x_slabs[4];
			float z_slabs[4];
			#ifdef Y_SLAB_COMPRESSION
			float y_min;
			float y_max;
			//With y compression:
			//-> 10 floats, 40 byte, 320 bit
			#else
			float y_min[4];
			float y_max[4];
			//Without y compression:
			//-> 16 floats, 64 byte, 512 bit
			#endif

			static patch_node from(const subd::patch_slab_node &from_node) {
				patch_node node;
				for (uint32_t i = 0; i < 4; ++i) {
					node.x_slabs[i] = from_node.x_slabs[i];
					node.z_slabs[i] = from_node.z_slabs[i];
			#ifndef Y_SLAB_COMPRESSION
					node.y_min[i] = from_node.y_min[i];
					node.y_max[i] = from_node.y_max[i];
			#endif
				}
			#ifdef Y_SLAB_COMPRESSION
				node.y_min = from_node.y_min;
				node.y_max = from_node.y_max;
			#endif
				return node;
			}
	
			#ifdef Y_SLAB_COMPRESSION
			float3 __device__ __forceinline__ get_min(uint32_t index) const {
				if (index == 0)			return { .x = x_slabs[0], .y = y_min, .z = z_slabs[0] };
				else if (index == 1)	return { .x = x_slabs[2], .y = y_min, .z = z_slabs[0] };
				else if (index == 2)	return { .x = x_slabs[0], .y = y_min, .z = z_slabs[2] };
				else					return { .x = x_slabs[2], .y = y_min, .z = z_slabs[2] };
			}

			float3 __device__ __forceinline__ get_max(uint32_t index) const {
				if (index == 0)			return { .x = x_slabs[1], .y = y_max, .z = z_slabs[1] };
				else if (index == 1)	return { .x = x_slabs[3], .y = y_max, .z = z_slabs[1] };
				else if (index == 2)	return { .x = x_slabs[1], .y = y_max, .z = z_slabs[3] };
				else					return { .x = x_slabs[3], .y = y_max, .z = z_slabs[3] };
			}
			#else

			float3 __device__ __forceinline__ get_min(uint32_t index) const {
				if (index == 0)			return { .x = x_slabs[0], .y = y_min[0], .z = z_slabs[0] };
				else if (index == 1)	return { .x = x_slabs[2], .y = y_min[1], .z = z_slabs[0] };
				else if (index == 2)	return { .x = x_slabs[0], .y = y_min[2], .z = z_slabs[2] };
				else					return { .x = x_slabs[2], .y = y_min[3], .z = z_slabs[2] };
			}

			float3 __device__ __forceinline__ get_max(uint32_t index) const {
				if (index == 0)			return { .x = x_slabs[1], .y = y_max[0], .z = z_slabs[1] };
				else if (index == 1)	return { .x = x_slabs[3], .y = y_max[1], .z = z_slabs[1] };
				else if (index == 2)	return { .x = x_slabs[1], .y = y_max[2], .z = z_slabs[3] };
				else					return { .x = x_slabs[3], .y = y_max[3], .z = z_slabs[3] };
			}
			#endif
		};
		#else
		struct __align__(16) patch_node {
			#ifdef Y_SLAB_COMPRESSION
			/*
				With y compression:
				Bit:	[  0|  1|  2|  3|  4|  5|  6|  7]
				xy_0	[x_slab_0	|x_slab_1	|y_min  ]
				xy_1	[x_slab_2	|x_slab_3	|       ]
				zy_0	[z_slab_0	|z_slab_1	|y_max  ]
				zy_1	[z_slab_2	|z_slab_3	|       ]
				-> 4 chars, 4 byte, 32 bit
			*/
			char box_data[4];
			#else
			/*
				Bit:	[  0|  1|  2|  3|  4|  5|  6|  7]
				xy_0	[x_slab_0	|x_slab_1	|y_min_0]
				xy_1	[x_slab_2	|x_slab_3	|y_min_1]
				zy_0	[z_slab_0	|z_slab_1	|y_min_2]
				zy_1	[z_slab_2	|z_slab_3	|y_min_3]
				y_max	[y_max_0|y_max_1|y_max_2|y_max_3]
				-> 5 chars, 5 byte, 40 bit
			*/
			char box_data[5];
			#endif

			static patch_node from(const subd::patch_slab_node &from_node) {
				patch_node node;
				uint32_t size = sizeof(node.box_data)/sizeof(node.box_data[0]);
				for (uint32_t i = 0; i < size; ++i)
					node.box_data[i] = from_node.box_data[i];
					
				return node;
			}
	
			#ifdef Y_SLAB_COMPRESSION
			float3 __device__ __forceinline__ get_min(uint32_t index, const aabb_f3 &parent_box) const {
				if (index == 0)			return { .x = x_slabs[0], .y = y_min, .z = z_slabs[0] };
				else if (index == 1)	return { .x = x_slabs[2], .y = y_min, .z = z_slabs[0] };
				else if (index == 2)	return { .x = x_slabs[0], .y = y_min, .z = z_slabs[2] };
				else					return { .x = x_slabs[2], .y = y_min, .z = z_slabs[2] };
			}

			float3 __device__ __forceinline__ get_max(uint32_t index, const aabb_f3 &parent_box) const {
				if (index == 0)			return { .x = x_slabs[1], .y = y_max, .z = z_slabs[1] };
				else if (index == 1)	return { .x = x_slabs[3], .y = y_max, .z = z_slabs[1] };
				else if (index == 2)	return { .x = x_slabs[1], .y = y_max, .z = z_slabs[3] };
				else					return { .x = x_slabs[3], .y = y_max, .z = z_slabs[3] };
			}
			#else

			float3 __device__ __forceinline__ get_min(uint32_t index, const aabb_f3 &parent_box) const {
				if (index == 0)			return { .x = x_slab(0, parent_box), .y = y_min(0, parent_box), .z = z_slab(0, parent_box) };
				else if (index == 1)	return { .x = x_slab(2, parent_box), .y = y_min(1, parent_box), .z = z_slab(0, parent_box) };
				else if (index == 2)	return { .x = x_slab(0, parent_box), .y = y_min(2, parent_box), .z = z_slab(2, parent_box) };
				else					return { .x = x_slab(2, parent_box), .y = y_min(3, parent_box), .z = z_slab(2, parent_box) };
			}

			float3 __device__ __forceinline__ get_max(uint32_t index, const aabb_f3 &parent_box) const {
				if (index == 0)			return { .x = x_slab(1, parent_box), .y = y_max(0, parent_box), .z = z_slab(1, parent_box) };
				else if (index == 1)	return { .x = x_slab(3, parent_box), .y = y_max(1, parent_box), .z = z_slab(1, parent_box) };
				else if (index == 2)	return { .x = x_slab(1, parent_box), .y = y_max(2, parent_box), .z = z_slab(3, parent_box) };
				else					return { .x = x_slab(3, parent_box), .y = y_max(3, parent_box), .z = z_slab(3, parent_box) };
			}
			#endif



			/*void set_x_slab(uint32_t index, float val, const aabb_f3 &parent_box) {
			}
			void set_z_slab(uint32_t index, float val, const aabb_f3 &parent_box) {
			}
			void set_y_min(uint32_t index, float val, const aabb_f3 &parent_box) {
			}
			void set_y_max(uint32_t index, float val, const aabb_f3 &parent_box) {
			}*/

			float __device__ __forceinline__ x_slab(uint32_t index, const aabb_f3 &parent_box) const {
				return 0.f;
			}
			float __device__ __forceinline__ z_slab(uint32_t index, const aabb_f3 &parent_box) const {
				return 0.f;
			}
			float __device__ __forceinline__ y_min(uint32_t index, const aabb_f3 &parent_box) const {
				return 0.f;
			}
			float __device__ __forceinline__ y_max(uint32_t index, const aabb_f3 &parent_box) const {
				return 0.f;
			}
		};
		#endif
	#else
		struct __align__(16) patch_node {
			// 4 slabs for xz -> 4 floats + 8 y coords -> 8 floats = 12 floats
			// instead of: 6 floats (24 byte)
			// keeping only inner slabs:
			// slab x_1 x_2
			// slab z_1 z_2
			//
			// boxes can be reconstructed by using the parent's slabs or root bounds

			// TODO: Combine quantization with other variants (slab-, half-slab- and no slab-compression)
		#ifndef QUANTIZATION
			float x_slabs[2];
			float z_slabs[2];
			#ifdef Y_SLAB_COMPRESSION
			float y_min;
			float y_max;
			//With y compression:
			//-> 6 floats, 24 byte, 192 bit
			#else
			float y_min[4];
			float y_max[4];
			//Without y compression:
			//-> 12 floats, 48 byte, 384 bit
			#endif
		#else
			#ifdef Y_SLAB_COMPRESSION
			/*
				With y compression:
				Bit:	[  0|  1|  2|  3|  4|  5|  6|  7]
				x_y0	[x_slab_0	|x_slab_1	|y_min  ]
				z_y1	[z_slab_0	|z_slab_1	|y_max  ]
				-> 2 chars, 2 byte, 16 bit
			*/
			char box_data[2];
			#else
			/*
				Bit:	[  0|  1|  2|  3|  4|  5|  6|  7]
				x_y0	[x_slab_0	|x_slab_1	|-free- ]
				z_y1	[z_slab_0	|z_slab_1	|-free- ]
				y2		[y_min_0|y_min_1|y_min_2|y_min_3]
				y3		[y_max_0|y_max_1|y_max_2|y_max_3]
				-> 4 chars, 4 byte, 32 bit
			*/
			char box_data[4];
			#endif

			//char xz_slabs[6];	// [x]
			//char y_min;			// contains: b0_min, b1_min, b2_min, b3_min, each 2 bit
			//char y_max;			// contains: b0_max, b1_max, b2_max, b3_max, each 2 bit
		#endif

        #ifndef QUANTIZATION
			static patch_node from(const subd::patch_slab_node &from_node) {
				patch_node node;
				for (uint32_t i = 0; i < 2; ++i) {
					node.x_slabs[i] = from_node.x_slabs[i];
					node.z_slabs[i] = from_node.z_slabs[i];
				}
		    #ifdef Y_SLAB_COMPRESSION
				node.y_min = from_node.y_min;
				node.y_max = from_node.y_max;
		    #else
				for (uint32_t i = 0; i < 4; ++i) {
					node.y_min[i] = from_node.y_min[i];
					node.y_max[i] = from_node.y_max[i];
				}
		    #endif
				return node;
			}
        #else
            static patch_node from(const subd::patch_slab_node &from_node) {
				patch_node node;
                //TODO: pass already quantized node here
                set_012(node.box_data[0], quantize_xz(from_node.x_slabs[0]));
                set_345(node.box_data[0], quantize_xz(from_node.x_slabs[1]));
                set_012(node.box_data[1], quantize_xz(from_node.z_slabs[0]));
                set_345(node.box_data[1], quantize_xz(from_node.z_slabs[1]));

            #ifdef Y_SLAB_COMPRESSION
                set_67(node.box_data[0], quantize_y(from_node.y_min));
                set_67(node.box_data[1], quantize_y(from_node.y_max));
            #else
                set_01(node.box_data[2], quantize_y(from_node.y_min[0]));
                set_23(node.box_data[2], quantize_y(from_node.y_min[1]));
                set_45(node.box_data[2], quantize_y(from_node.y_min[2]));
                set_67(node.box_data[2], quantize_y(from_node.y_min[3]));
                set_01(node.box_data[3], quantize_y(from_node.y_max[0]));
                set_23(node.box_data[3], quantize_y(from_node.y_max[1]));
                set_45(node.box_data[3], quantize_y(from_node.y_max[2]));
                set_67(node.box_data[3], quantize_y(from_node.y_max[3]));
            #endif

				/*for (uint32_t i = 0; i < 2; ++i) {
					node.x_slabs[i] = from_node.x_slabs[i];
					node.z_slabs[i] = from_node.z_slabs[i];
				}
		    #ifdef Y_SLAB_COMPRESSION
				node.y_min = from_node.y_min;
				node.y_max = from_node.y_max;
		    #else
				for (uint32_t i = 0; i < 4; ++i) {
					node.y_min[i] = from_node.y_min[i];
					node.y_max[i] = from_node.y_max[i];
				}
		    #endif*/

				return node;
			}
        #endif
		};
	#endif
#endif
    }
}