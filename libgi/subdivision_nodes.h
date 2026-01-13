#pragma once

#include "driver/defines.h"
//#include <iostream> // only TMP/DBG

#ifdef __CUDACC__
#define heterogeneous __host__ __device__
#else
#define heterogeneous
#endif

namespace subd {
	struct patch_base_node {
		aabb boxes[4];
	};

	template<typename AABB>
	inline heterogeneous AABB make_aabb(float x0, float y0, float z0, float x1, float y1, float z1) {
		return AABB{ {x0, y0, z0}, {x1, y1, z1} };
	}

#ifdef QUANTIZATION
	#define SIZE_XZ 8
	#define THRESHOLDS_XZ_VALUES 0.f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f
	#define SIZE_Y 4
	#define THRESHOLDS_Y_VALUES 0.f, 0.25f, 0.5f, 0.75f
	static uint8_t quantize_xz(float val) {
		constexpr float thresholds_xz[SIZE_XZ] = { THRESHOLDS_XZ_VALUES };
		uint8_t res = 0;
		for (uint32_t i = 0; i < SIZE_XZ; ++i) {
			if (thresholds_xz[i] <= val) res = i;
			else break;
		}
		return res;
	}
	static uint8_t quantize_y(float val) {
		constexpr float thresholds_y[SIZE_Y] = { THRESHOLDS_Y_VALUES };
		uint8_t res = 0;
		for (uint32_t i = 0; i < SIZE_Y; ++i) {
			if (thresholds_y[i] <= val) res = i;
			else break;
		}
		return res;
	}
	static heterogeneous inline float dequantize_xz(uint8_t val) {
		constexpr float thresholds_xz[SIZE_XZ] = { THRESHOLDS_XZ_VALUES };
		return thresholds_xz[val];
	}
	static heterogeneous inline float dequantize_y(uint8_t val) {
		constexpr float thresholds_y[SIZE_Y] = { THRESHOLDS_Y_VALUES };
		return thresholds_y[val];
	}

	inline void set_012(uint8_t &field, uint8_t val) { field = (field & 0b00011111 | val << 5); }
	inline void set_345(uint8_t &field, uint8_t val) { field = (field & 0b11100011 | val << 2); }
	inline void set_67 (uint8_t &field, uint8_t val) { field = (field & 0b11111100 | val); }

	inline uint8_t heterogeneous get_012(const uint8_t &field) { return ((field & 0b11100000) >> 5); }
	inline uint8_t heterogeneous get_345(const uint8_t &field) { return ((field & 0b00011100) >> 2); }
	inline uint8_t heterogeneous get_67 (const uint8_t &field) { return (field & 0b00000011); }
	
	#ifndef Y_SLAB_COMPRESSION
	inline void set_01 (uint8_t &field, uint8_t val) { field = (field & 0b00111111 | val << 6); }
	inline void set_23 (uint8_t &field, uint8_t val) { field = (field & 0b11001111 | val << 4); }
	inline void set_45 (uint8_t &field, uint8_t val) { field = (field & 0b11110011 | val << 2); }

	inline uint8_t heterogeneous get_01 (const uint8_t &field) { return ((field & 0b11000000) >> 6); }
	inline uint8_t heterogeneous get_23 (const uint8_t &field) { return ((field & 0b00110000) >> 4); }
	inline uint8_t heterogeneous get_45 (const uint8_t &field) { return ((field & 0b00001100) >> 2); }
	#endif
#endif


#ifndef SLAB_COMPRESSION
	#ifndef QUANTIZATION
		template<typename F4>
		struct patch_slab_node {
			// memory layout and order of the following fields is important!
			F4 min_1;
			F4 min_2;
			F4 min_3;
			F4 max_1;
			F4 max_2;
			F4 max_3;
			//-> 24 floats, 96 byte, 768 bit
			//non optimized layout:
			//-> 32 floats, 128 byte, 1024 bit

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

			template<typename AABB>
			AABB heterogeneous get_box(uint32_t index) const {
				//assert(index <= 3);

				if (index == 0)			return make_aabb<AABB>(min_1.x, min_1.y, min_1.z,
														 max_1.x, max_1.y, max_1.z);
				else if (index == 1)	return make_aabb<AABB>(min_1.w, min_2.x, min_2.y,
													 	max_1.w, max_2.x, max_2.y);
				else if (index == 2)	return make_aabb<AABB>(min_2.z, min_2.w, min_3.x,
														 max_2.z, max_2.w, max_3.x);
				else					return make_aabb<AABB>(min_3.y, min_3.z, min_3.w,
														 max_3.y, max_3.z, max_3.w);
			}
		};
	#else
		struct patch_slab_node {
			/*
				Bit:	[  0|  1|  2|  3|  4|  5|  6|  7]
				b0min	[0_x_min	|0_z_min	|0_y_min]
				b0max	[0_x_max	|0_z_max	|0_y_max]
				b1min	[1_x_min	|1_z_min	|1_y_min]
				b1max	[1_x_max	|1_z_max	|1_y_max]
				b2min	[2_x_min	|2_z_min	|2_y_min]
				b2max	[2_x_max	|2_z_max	|2_y_max]
				b3min	[3_x_min	|3_z_min	|3_y_min]
				b3max	[3_x_max	|3_z_max	|3_y_max]
				-> 8 chars, 8 byte, 64 bit
			*/
			uint8_t box_data[8];

			inline void set_x_min(uint32_t index, float val, const aabb &parent_box) {
				double x_f = 1.0 / (parent_box.max.x - parent_box.min.x);
				uint8_t q_val = quantize_xz((val - parent_box.min.x) * x_f);
				set_012(box_data[index << 1], q_val);
			}
			inline void set_x_max(uint32_t index, float val, const aabb &parent_box) {
				double x_f = 1.0 / (parent_box.max.x - parent_box.min.x);
				uint8_t q_val = quantize_xz((parent_box.max.x - val) * x_f);
				set_012(box_data[(index << 1) + 1], q_val);
			}
			inline void set_z_min(uint32_t index, float val, const aabb &parent_box) {
				double z_f = 1.0 / (parent_box.max.z - parent_box.min.z);
				uint8_t q_val = quantize_xz((val - parent_box.min.z) * z_f);
				set_345(box_data[index << 1], q_val);
			}
			inline void set_z_max(uint32_t index, float val, const aabb &parent_box) {
				double z_f = 1.0 / (parent_box.max.z - parent_box.min.z);
				uint8_t q_val = quantize_xz((parent_box.max.z - val) * z_f);
				set_345(box_data[(index << 1) + 1], q_val);
			}
			inline void set_y_min(uint32_t index, float val, const aabb &parent_box) {
				double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
				uint8_t q_val = quantize_y((val - parent_box.min.y) * y_f);
				set_67(box_data[index << 1], q_val);
			}
			inline void set_y_max(uint32_t index, float val, const aabb &parent_box) {
				double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
				uint8_t q_val = quantize_y((parent_box.max.y - val) * y_f);
				set_67(box_data[(index << 1) + 1], q_val);
			}

			template<typename AABB>
			inline float heterogeneous x_min(uint32_t index, const AABB &parent_box) const {
				float dim = parent_box.max.x - parent_box.min.x;
				return dequantize_xz(get_012(box_data[index<<1])) * dim + parent_box.min.x;
			}
			template<typename AABB>
			inline float heterogeneous x_max(uint32_t index, const AABB &parent_box) const {
				float dim = parent_box.max.x - parent_box.min.x;
				return (1.f - dequantize_xz(get_012(box_data[(index<<1) + 1]))) * dim + parent_box.min.x;
			}
			template<typename AABB>
			inline float heterogeneous z_min(uint32_t index, const AABB &parent_box) const {
				float dim = parent_box.max.z - parent_box.min.z;
				return dequantize_xz(get_345(box_data[index<<1])) * dim + parent_box.min.z;
			}
			template<typename AABB>
			inline float heterogeneous z_max(uint32_t index, const AABB &parent_box) const {
				float dim = parent_box.max.z - parent_box.min.z;
				return (1.f - dequantize_xz(get_345(box_data[(index<<1) + 1]))) * dim + parent_box.min.z;
			}
			template<typename AABB>
			inline float heterogeneous y_min(uint32_t index, const AABB &parent_box) const {
				float dim = parent_box.max.y - parent_box.min.y;
				return dequantize_y(get_67(box_data[index<<1])) * dim + parent_box.min.y;
			}
			template<typename AABB>
			inline float heterogeneous y_max(uint32_t index, const AABB &parent_box) const {
				float dim = parent_box.max.y - parent_box.min.y;
				return (1.f - dequantize_y(get_67(box_data[(index<<1) + 1]))) * dim + parent_box.min.y;
			}

			static patch_slab_node from(const patch_base_node &from_node, const aabb &parent_box) {
				patch_slab_node node;
				for (uint32_t i = 0; i < 4; ++i) {
					const auto &box = from_node.boxes[i];
					node.set_x_min(i, box.min.x, parent_box);
					node.set_x_max(i, box.max.x, parent_box);
					node.set_z_min(i, box.min.z, parent_box);
					node.set_z_max(i, box.max.z, parent_box);
					node.set_y_min(i, box.min.y, parent_box);
					node.set_y_max(i, box.max.y, parent_box);
				}
			}

			static patch_slab_node copy(const subd::patch_slab_node &from_node) {
				patch_slab_node node;
				uint32_t size = sizeof(node.box_data)/sizeof(node.box_data[0]);
				for (uint32_t i = 0; i < size; ++i)
					node.box_data[i] = from_node.box_data[i];
					
				return node;
			}

			template<typename AABB>
			AABB heterogeneous get_box(uint32_t index, AABB parent_box) const {
				//assert(index <= 3);
				return make_aabb<AABB>(x_min(index, parent_box), y_min(index, parent_box), z_min(index, parent_box),
									   x_max(index, parent_box), y_max(index, parent_box), z_max(index, parent_box));
				
			}
		};
	#endif
#endif
#ifdef SLAB_COMPRESSION
	#ifndef HALF_SLAB_COMPRESSION
		#ifndef QUANTIZATION
			struct patch_slab_node {
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
				#else
				float y_min[4];
				float y_max[4];
				#endif

				//static patch_slab_node from(const patch_base_node &from_node);
				static patch_slab_node from(const patch_base_node &from_node) {
					patch_slab_node node;
					node.x_slabs[0] = std::min(from_node.boxes[0].min.x, from_node.boxes[2].min.x);
					node.x_slabs[1] = std::max(from_node.boxes[0].max.x, from_node.boxes[2].max.x);
					node.x_slabs[2] = std::min(from_node.boxes[1].min.x, from_node.boxes[3].min.x);
					node.x_slabs[3] = std::max(from_node.boxes[1].max.x, from_node.boxes[3].max.x);
					node.z_slabs[0] = std::min(from_node.boxes[0].min.z, from_node.boxes[1].min.z);
					node.z_slabs[1] = std::max(from_node.boxes[0].max.z, from_node.boxes[1].max.z);
					node.z_slabs[2] = std::min(from_node.boxes[2].min.z, from_node.boxes[3].min.z);
					node.z_slabs[3] = std::max(from_node.boxes[2].max.z, from_node.boxes[3].max.z);

				#ifdef Y_SLAB_COMPRESSION
					node.y_min = std::min(
									std::min(from_node.boxes[0].min.y, from_node.boxes[1].min.y),
									std::min(from_node.boxes[2].min.y, from_node.boxes[3].min.y)
								);
					node.y_max = std::max(
									std::max(from_node.boxes[0].max.y, from_node.boxes[1].max.y),
									std::max(from_node.boxes[2].max.y, from_node.boxes[3].max.y)
								);
				#else
					node.y_min[0] = from_node.boxes[0].min.y;
					node.y_min[1] = from_node.boxes[1].min.y;
					node.y_min[2] = from_node.boxes[2].min.y;
					node.y_min[3] = from_node.boxes[3].min.y;
					node.y_max[0] = from_node.boxes[0].max.y;
					node.y_max[1] = from_node.boxes[1].max.y;
					node.y_max[2] = from_node.boxes[2].max.y;
					node.y_max[3] = from_node.boxes[3].max.y;
				#endif

					return node;
				}

				static patch_slab_node copy(const subd::patch_slab_node &from_node) {
					patch_slab_node node;
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

				//aabb get_box(uint32_t index) const;
				template<typename AABB>
				AABB heterogeneous inline get_box(uint32_t index) const {
					//assert(index <= 3);
				#ifdef Y_SLAB_COMPRESSION
						if (index == 0)		 return make_aabb<AABB>(x_slabs[0], y_min, z_slabs[0],
																	x_slabs[1], y_max, z_slabs[1]);
						else if (index == 1) return make_aabb<AABB>(x_slabs[2], y_min, z_slabs[0],
																	x_slabs[3], y_max, z_slabs[1]);
						else if (index == 2) return make_aabb<AABB>(x_slabs[0], y_min, z_slabs[2],
																	x_slabs[1], y_max, z_slabs[3]);
						else				 return make_aabb<AABB>(x_slabs[2], y_min, z_slabs[2],
																	x_slabs[3], y_max, z_slabs[3]);
				#else
						if (index == 0)		 return make_aabb<AABB>(x_slabs[0], y_min[0], z_slabs[0],
																	x_slabs[1], y_max[0], z_slabs[1]);
						else if (index == 1) return make_aabb<AABB>(x_slabs[2], y_min[1], z_slabs[0],
																	x_slabs[3], y_max[1], z_slabs[1]);
						else if (index == 2) return make_aabb<AABB>(x_slabs[0], y_min[2], z_slabs[2],
																	x_slabs[1], y_max[2], z_slabs[3]);
						else				 return make_aabb<AABB>(x_slabs[2], y_min[3], z_slabs[2],
																	x_slabs[3], y_max[3], z_slabs[3]);
				#endif
				}
			};


		#else
			struct patch_slab_node {
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
				uint8_t box_data[4];
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
				uint8_t box_data[5];
			#endif

				inline void set_x_slab(uint32_t index, float val, const aabb &parent_box) {
					double x_f = 1.0 / (parent_box.max.x - parent_box.min.x);
					//if (!std::isfinite(x_f)) x_f = FLT_MIN;
					switch (index) {
						case 0: set_012(box_data[0], quantize_xz((val - parent_box.min.x) * x_f)); break;
						case 1: set_345(box_data[0], quantize_xz((parent_box.max.x - val) * x_f)); break;
						case 2: set_012(box_data[1], quantize_xz((val - parent_box.min.x) * x_f)); break;
						case 3: set_345(box_data[1], quantize_xz((parent_box.max.x - val) * x_f)); break;
					}
				}
				inline void set_z_slab(uint32_t index, float val, const aabb &parent_box) {
					double z_f = 1.0 / (parent_box.max.z - parent_box.min.z);
					//if (!std::isfinite(z_f)) z_f = FLT_MIN;
					switch (index) {
						case 0: set_012(box_data[2], quantize_xz((val - parent_box.min.z) * z_f)); break;
						case 1: set_345(box_data[2], quantize_xz((parent_box.max.z - val) * z_f)); break;
						case 2: set_012(box_data[3], quantize_xz((val - parent_box.min.z) * z_f)); break;
						case 3: set_345(box_data[3], quantize_xz((parent_box.max.z - val) * z_f)); break;
					}
				}
			#ifdef Y_SLAB_COMPRESSION
				inline void set_y_min(float val, const aabb &parent_box) {
					double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
					//if (!std::isfinite(y_f)) y_f = FLT_MIN;
					uint8_t q_val = quantize_y((val - parent_box.min.y) * y_f);
					set_67(box_data[0], q_val);
				}
				inline void set_y_max(float val, const aabb &parent_box) {
					double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
					//if (!std::isfinite(y_f)) y_f = FLT_MIN;
					uint8_t q_val = quantize_y((parent_box.max.y - val) * y_f);
					set_67(box_data[2], q_val);
				}
			#else
				inline void set_y_min(uint32_t index, float val, const aabb &parent_box) {
					double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
					//if (!std::isfinite(y_f)) y_f = FLT_MIN;
					uint8_t q_val = quantize_y((val - parent_box.min.y) * y_f);
					switch (index) {
						case 0: set_67(box_data[0], q_val); break;
						case 1: set_67(box_data[1], q_val); break;
						case 2: set_67(box_data[2], q_val); break;
						case 3: set_67(box_data[3], q_val); break;
					}
				}
				inline void set_y_max(uint32_t index, float val, const aabb &parent_box) {
					double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
					//if (!std::isfinite(y_f)) y_f = FLT_MIN;
					uint8_t q_val = quantize_y((parent_box.max.y - val) * y_f);
					switch (index) {
						case 0: set_01(box_data[4], q_val); break;
						case 1: set_23(box_data[4], q_val); break;
						case 2: set_45(box_data[4], q_val); break;
						case 3: set_67(box_data[4], q_val); break;
					}
				}
			#endif

				template<typename AABB>
				inline float heterogeneous x_slab(uint32_t index, const AABB &parent_box) const {
					float dim = parent_box.max.x - parent_box.min.x;
					switch (index) {
						case 0: return dequantize_xz(get_012(box_data[0])) * dim + parent_box.min.x;
						case 1: return (1.f - dequantize_xz(get_345(box_data[0]))) * dim + parent_box.min.x;
						case 2: return dequantize_xz(get_012(box_data[1])) * dim + parent_box.min.x;
						case 3: return (1.f - dequantize_xz(get_345(box_data[1]))) * dim + parent_box.min.x;
					}
					assert(false);
				}
				template<typename AABB>
				inline float heterogeneous z_slab(uint32_t index, const AABB &parent_box) const {
					float dim = parent_box.max.z - parent_box.min.z;
					switch (index) {
						case 0: return dequantize_xz(get_012(box_data[2])) * dim + parent_box.min.z;
						case 1: return (1.f - dequantize_xz(get_345(box_data[2]))) * dim + parent_box.min.z;
						case 2: return dequantize_xz(get_012(box_data[3])) * dim + parent_box.min.z;
						case 3: return (1.f - dequantize_xz(get_345(box_data[3]))) * dim + parent_box.min.z;
					}
					assert(false);
				}
			
			#ifdef Y_SLAB_COMPRESSION
				template<typename AABB>
				inline float heterogeneous y_min(const AABB &parent_box) const {
					float dim = parent_box.max.y - parent_box.min.y;
					return dequantize_y(get_67(box_data[0])) * dim + parent_box.min.y;
				}
				template<typename AABB>
				inline float heterogeneous y_max(const AABB &parent_box) const {
					float dim = parent_box.max.y - parent_box.min.y;
					return (1.f - dequantize_y(get_67(box_data[2]))) * dim + parent_box.min.y;
				}
			#else
				template<typename AABB>
				inline float heterogeneous y_min(uint32_t index, const AABB &parent_box) const {
					float dim = parent_box.max.y - parent_box.min.y;
					switch (index) {
						case 0: return dequantize_y(get_67(box_data[0])) * dim + parent_box.min.y;
						case 1: return dequantize_y(get_67(box_data[1])) * dim + parent_box.min.y;
						case 2: return dequantize_y(get_67(box_data[2])) * dim + parent_box.min.y;
						case 3: return dequantize_y(get_67(box_data[3])) * dim + parent_box.min.y;
					}
					assert(false);
				}
				template<typename AABB>
				inline float heterogeneous y_max(uint32_t index, const AABB &parent_box) const {
					float dim = parent_box.max.y - parent_box.min.y;
					switch (index) {
						case 0: return (1.f - dequantize_y(get_01(box_data[4]))) * dim + parent_box.min.y;
						case 1: return (1.f - dequantize_y(get_23(box_data[4]))) * dim + parent_box.min.y;
						case 2: return (1.f - dequantize_y(get_45(box_data[4]))) * dim + parent_box.min.y;
						case 3: return (1.f - dequantize_y(get_67(box_data[4]))) * dim + parent_box.min.y;
					}
					assert(false);
				}
			#endif

				static patch_slab_node from(const patch_base_node &from_node, const aabb &parent_box) {
					patch_slab_node node;
					node.set_x_slab(0, std::min(from_node.boxes[0].min.x, from_node.boxes[2].min.x), parent_box);
					// TMP/DBG:
					float x_slab_0 = std::min(from_node.boxes[0].min.x, from_node.boxes[2].min.x);
					//std::cout << "x_slab_0 (ref): " << x_slab_0 << std::endl;
					//std::cout << "x_slab_0 (qnt): " << node.x_slab(0, parent_box) << std::endl << std::endl;

					float x_slab_1 = std::max(from_node.boxes[0].max.x, from_node.boxes[2].max.x);
					node.set_x_slab(1, std::max(from_node.boxes[0].max.x, from_node.boxes[2].max.x), parent_box);
					//std::cout << "x_slab_1 (ref): " << x_slab_1 << std::endl;
					//std::cout << "x_slab_1 (qnt): " << node.x_slab(1, parent_box) << std::endl << std::endl;

					node.set_x_slab(2, std::min(from_node.boxes[1].min.x, from_node.boxes[3].min.x), parent_box);
					node.set_x_slab(3, std::max(from_node.boxes[1].max.x, from_node.boxes[3].max.x), parent_box);
					node.set_z_slab(0, std::min(from_node.boxes[0].min.z, from_node.boxes[1].min.z), parent_box);

					float z_slab_1 = std::max(from_node.boxes[0].max.z, from_node.boxes[1].max.z);
					node.set_z_slab(1, std::max(from_node.boxes[0].max.z, from_node.boxes[1].max.z), parent_box);
					//std::cout << "z_slab_1 (ref): " << z_slab_1 << std::endl;
					//std::cout << "z_slab_1 (qnt): " << node.z_slab(1, parent_box) << std::endl << std::endl;

					node.set_z_slab(2, std::min(from_node.boxes[2].min.z, from_node.boxes[3].min.z), parent_box);
					node.set_z_slab(3, std::max(from_node.boxes[2].max.z, from_node.boxes[3].max.z), parent_box);

				#ifdef Y_SLAB_COMPRESSION
					float y_min = std::min(
									std::min(from_node.boxes[0].min.y, from_node.boxes[1].min.y),
									std::min(from_node.boxes[2].min.y, from_node.boxes[3].min.y)
								);
					node.set_y_min(y_min, parent_box);
					float y_max = std::max(
									std::max(from_node.boxes[0].max.y, from_node.boxes[1].max.y),
									std::max(from_node.boxes[2].max.y, from_node.boxes[3].max.y)
								);
					node.set_y_max(y_max, parent_box);
				#else
					float y_min_0 = from_node.boxes[0].min.y;
					node.set_y_min(0, from_node.boxes[0].min.y, parent_box);
					//std::cout << "y_min_0 (ref): " << y_min_0 << std::endl;
					//std::cout << "y_min_0 (qnt): " << node.y_min(0, parent_box) << std::endl << std::endl;

					node.set_y_min(1, from_node.boxes[1].min.y, parent_box);
					node.set_y_min(2, from_node.boxes[2].min.y, parent_box);
					node.set_y_min(3, from_node.boxes[3].min.y, parent_box);

					float y_max_0 = from_node.boxes[0].max.y;
					node.set_y_max(0, from_node.boxes[0].max.y, parent_box);
					//std::cout << "y_max_0 (ref): " << y_max_0 << std::endl;
					//std::cout << "y_max_0 (qnt): " << node.y_max(0, parent_box) << std::endl << std::endl;

					float y_max_1 = from_node.boxes[1].max.y;
					node.set_y_max(1, from_node.boxes[1].max.y, parent_box);
					//std::cout << "y_max_1 (ref): " << y_max_1 << std::endl;
					//std::cout << "y_max_1 (qnt): " << node.y_max(1, parent_box) << std::endl << std::endl;

					node.set_y_max(2, from_node.boxes[2].max.y, parent_box);
					node.set_y_max(3, from_node.boxes[3].max.y, parent_box);
				#endif

					return node;
				}

				static patch_slab_node copy(const subd::patch_slab_node &from_node) {
					patch_slab_node node;
					uint32_t size = sizeof(node.box_data)/sizeof(node.box_data[0]);
					for (uint32_t i = 0; i < size; ++i)
						node.box_data[i] = from_node.box_data[i];
						
					return node;
				}

				template<typename AABB>
				AABB heterogeneous inline get_box(uint32_t index, const AABB &parent_box) const {
					//assert(index <= 3);
				#ifdef Y_SLAB_COMPRESSION
					if (index == 0)			return { make_aabb<AABB>(x_slab(0, parent_box), y_min(parent_box), z_slab(0, parent_box),
																	 x_slab(1, parent_box), y_max(parent_box), z_slab(1, parent_box)) };
					else if (index == 1)	return { make_aabb<AABB>(x_slab(2, parent_box), y_min(parent_box), z_slab(0, parent_box),
																	 x_slab(3, parent_box), y_max(parent_box), z_slab(1, parent_box)) };
					else if (index == 2)	return { make_aabb<AABB>(x_slab(0, parent_box), y_min(parent_box), z_slab(2, parent_box),
																	 x_slab(1, parent_box), y_max(parent_box), z_slab(3, parent_box)) };
					else					return { make_aabb<AABB>(x_slab(2, parent_box), y_min(parent_box), z_slab(2, parent_box),
																	 x_slab(3, parent_box), y_max(parent_box), z_slab(3, parent_box)) };
				#else
					if (index == 0)			return { make_aabb<AABB>(x_slab(0, parent_box), y_min(0, parent_box), z_slab(0, parent_box),
																	 x_slab(1, parent_box), y_max(0, parent_box), z_slab(1, parent_box)) };
					else if (index == 1)	return { make_aabb<AABB>(x_slab(2, parent_box), y_min(1, parent_box), z_slab(0, parent_box),
																	 x_slab(3, parent_box), y_max(1, parent_box), z_slab(1, parent_box)) };
					else if (index == 2)	return { make_aabb<AABB>(x_slab(0, parent_box), y_min(2, parent_box), z_slab(2, parent_box),
																	 x_slab(1, parent_box), y_max(2, parent_box), z_slab(3, parent_box)) };
					else					return { make_aabb<AABB>(x_slab(2, parent_box), y_min(3, parent_box), z_slab(2, parent_box),
																	 x_slab(3, parent_box), y_max(3, parent_box), z_slab(3, parent_box)) };
				#endif
				}
			};
		#endif


	#else
		#ifndef QUANTIZATION
			// 4 slabs for xz -> 4 floats + 8 y coords -> 8 floats = 12 floats
			// instead of: 6 floats (24 byte)
			// keeping only inner slabs:
			// slab x_1 x_2
			// slab z_1 z_2
			//
			// boxes can be reconstructed by using the parent box
			// (outdated: boxes can be reconstructed independently of the parent box by using the parent's slabs or root bounds)
			struct patch_slab_node {
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

				static patch_slab_node from(const patch_base_node &from_node) {
					patch_slab_node node;
					node.x_slabs[0] = std::max(from_node.boxes[0].max.x, from_node.boxes[2].max.x);
					node.x_slabs[1] = std::min(from_node.boxes[1].min.x, from_node.boxes[3].min.x);
					node.z_slabs[0] = std::max(from_node.boxes[0].max.z, from_node.boxes[1].max.z);
					node.z_slabs[1] = std::min(from_node.boxes[2].min.z, from_node.boxes[3].min.z);

			#ifdef Y_SLAB_COMPRESSION
					node.y_min = std::min(
									std::min(from_node.boxes[0].min.y, from_node.boxes[1].min.y),
									std::min(from_node.boxes[2].min.y, from_node.boxes[3].min.y)
								);
					node.y_max = std::max(
									std::max(from_node.boxes[0].max.y, from_node.boxes[1].max.y),
									std::max(from_node.boxes[2].max.y, from_node.boxes[3].max.y)
								);
			#else
					node.y_min[0] = from_node.boxes[0].min.y;
					node.y_min[1] = from_node.boxes[1].min.y;
					node.y_min[2] = from_node.boxes[2].min.y;
					node.y_min[3] = from_node.boxes[3].min.y;
					node.y_max[0] = from_node.boxes[0].max.y;
					node.y_max[1] = from_node.boxes[1].max.y;
					node.y_max[2] = from_node.boxes[2].max.y;
					node.y_max[3] = from_node.boxes[3].max.y;
			#endif

					return node;
				}

				static patch_slab_node copy(const subd::patch_slab_node &from_node) {
					patch_slab_node node;
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

				template<typename AABB>
				AABB heterogeneous inline get_box(uint32_t index, AABB parent_box) const {
					//assert(index <= 3);
				#ifdef Y_SLAB_COMPRESSION
					if (index == 0)         return make_aabb<AABB>(parent_box.min.x, y_min,  parent_box.min.z,
																   x_slabs[0],       y_max,  z_slabs[0]);
					else if (index == 1)    return make_aabb<AABB>(x_slabs[1],       y_min,  parent_box.min.z,
																   parent_box.max.x, y_max,  z_slabs[0]);
					else if (index == 2)    return make_aabb<AABB>(parent_box.min.x, y_min,  z_slabs[1],
																   x_slabs[0],       y_max,  parent_box.max.z);
					else                    return make_aabb<AABB>(x_slabs[1],       y_min,  z_slabs[1],
																   parent_box.max.x, y_max,  parent_box.max.z);
				#else
					if (index == 0)         return make_aabb<AABB>(parent_box.min.x, y_min[0],  parent_box.min.z,
																   x_slabs[0],       y_max[0],  z_slabs[0]);
					else if (index == 1)    return make_aabb<AABB>(x_slabs[1],       y_min[1],  parent_box.min.z,
																   parent_box.max.x, y_max[1],  z_slabs[0]);
					else if (index == 2)    return make_aabb<AABB>(parent_box.min.x, y_min[2],  z_slabs[1],
																   x_slabs[0],       y_max[2],  parent_box.max.z);
					else                    return make_aabb<AABB>(x_slabs[1],       y_min[3],  z_slabs[1],
																   parent_box.max.x, y_max[3],  parent_box.max.z);
				#endif
				}
			};

			
		#else
			struct patch_slab_node {
			#ifdef Y_SLAB_COMPRESSION
				/*
					With y compression:
					Bit:	[  0|  1|  2|  3|  4|  5|  6|  7]
					x_y0	[x_slab_0	|x_slab_1	|y_min  ]
					z_y1	[z_slab_0	|z_slab_1	|y_max  ]
					-> 2 chars, 2 byte, 16 bit
				*/
				uint8_t box_data[2];
			#else
				/*
					Bit:	[  0|  1|  2|  3|  4|  5|  6|  7]
					x_y0	[x_slab_0	|x_slab_1	|-free- ]
					z_y1	[z_slab_0	|z_slab_1	|-free- ]
					y2		[y_min_0|y_min_1|y_min_2|y_min_3]
					y3		[y_max_0|y_max_1|y_max_2|y_max_3]
					-> 4 chars, 4 byte, 32 bit
				*/
				uint8_t box_data[4];
			#endif

				inline void set_x_slab(uint32_t index, float val, const aabb &parent_box) {
					double x_f = 1.0 / (parent_box.max.x - parent_box.min.x);
					//if (!std::isfinite(x_f)) x_f = FLT_MIN;
					switch (index) {
						case 0: set_012(box_data[0], quantize_xz((parent_box.max.x - val) * x_f)); break;
						case 1: set_345(box_data[0], quantize_xz((val - parent_box.min.x) * x_f)); break;
					}

				}
				inline void set_z_slab(uint32_t index, float val, const aabb &parent_box) {
					double z_f = 1.0 / (parent_box.max.z - parent_box.min.z);
					//if (!std::isfinite(z_f)) z_f = FLT_MIN;
					switch (index) {
						case 0: set_012(box_data[1], quantize_xz((parent_box.max.z - val) * z_f)); break;
						case 1: set_345(box_data[1], quantize_xz((val - parent_box.min.z) * z_f)); break;
					}
				}
			#ifdef Y_SLAB_COMPRESSION
				inline void set_y_min(float val, const aabb &parent_box) {
					double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
					//if (!std::isfinite(y_f)) y_f = FLT_MIN;
					uint8_t q_val = quantize_y((val - parent_box.min.y) * y_f);
					set_67(box_data[0], q_val);
				}
				inline void set_y_max(float val, const aabb &parent_box) {
					double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
					//if (!std::isfinite(y_f)) y_f = FLT_MIN;
					uint8_t q_val = quantize_y((parent_box.max.y - val) * y_f);
					set_67(box_data[1], q_val);
				}
			#else
				inline void set_y_min(uint32_t index, float val, const aabb &parent_box) {
					double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
					uint8_t q_val = quantize_y((val - parent_box.min.y) * y_f);
					switch (index) {
						case 0: set_01(box_data[2], q_val); break;
						case 1: set_23(box_data[2], q_val); break;
						case 2: set_45(box_data[2], q_val); break;
						case 3: set_67(box_data[2], q_val); break;
					}
				}
				inline void set_y_max(uint32_t index, float val, const aabb &parent_box) {
					double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
					uint8_t q_val = quantize_y((parent_box.max.y - val) * y_f);
					switch (index) {
						case 0: set_01(box_data[3], q_val); break;
						case 1: set_23(box_data[3], q_val); break;
						case 2: set_45(box_data[3], q_val); break;
						case 3: set_67(box_data[3], q_val); break;
					}
				}
			#endif

				template<typename AABB>
				inline float heterogeneous x_slab(uint32_t index, const AABB &parent_box) const {
					float dim = parent_box.max.x - parent_box.min.x;
					switch (index) {
						case 0: return (1.f - dequantize_xz(get_012(box_data[0]))) * dim + parent_box.min.x;
						case 1: return dequantize_xz(get_345(box_data[0])) * dim + parent_box.min.x;
					}
					assert(false);
				}
				template<typename AABB>
				inline float heterogeneous z_slab(uint32_t index, const AABB &parent_box) const {
					float dim = parent_box.max.z - parent_box.min.z;
					switch (index) {
						case 0: return (1.f - dequantize_xz(get_012(box_data[1]))) * dim + parent_box.min.z;
						case 1: return dequantize_xz(get_345(box_data[1])) * dim + parent_box.min.z;
					}
					assert(false);
				}
			#ifdef Y_SLAB_COMPRESSION
				template<typename AABB>
				inline float heterogeneous y_min(const AABB &parent_box) const {
					float dim = parent_box.max.y - parent_box.min.y;
					return dequantize_y(get_67(box_data[0])) * dim + parent_box.min.y;
				}
				template<typename AABB>
				inline float heterogeneous y_max(const AABB &parent_box) const {
					float dim = parent_box.max.y - parent_box.min.y;
					return (1.f - dequantize_y(get_67(box_data[1]))) * dim + parent_box.min.y;
				}
			#else
				template<typename AABB>
				inline float heterogeneous y_min(uint32_t index, const AABB &parent_box) const {
					float dim = parent_box.max.y - parent_box.min.y;
					switch (index) {
						case 0: return dequantize_y(get_01(box_data[2])) * dim + parent_box.min.y;
						case 1: return dequantize_y(get_23(box_data[2])) * dim + parent_box.min.y;
						case 2: return dequantize_y(get_45(box_data[2])) * dim + parent_box.min.y;
						case 3: return dequantize_y(get_67(box_data[2])) * dim + parent_box.min.y;
					}
					assert(false);
				}
				template<typename AABB>
				inline float heterogeneous y_max(uint32_t index, const AABB &parent_box) const {
					float dim = parent_box.max.y - parent_box.min.y;
					switch (index) {
						case 0: return (1.f - dequantize_y(get_01(box_data[3]))) * dim + parent_box.min.y;
						case 1: return (1.f - dequantize_y(get_23(box_data[3]))) * dim + parent_box.min.y;
						case 2: return (1.f - dequantize_y(get_45(box_data[3]))) * dim + parent_box.min.y;
						case 3: return (1.f - dequantize_y(get_67(box_data[3]))) * dim + parent_box.min.y;
					}
					assert(false);
				}
			#endif

				static patch_slab_node from(const patch_base_node &from_node, const aabb &parent_box) {
					patch_slab_node node;
					node.set_x_slab(0, std::max(from_node.boxes[0].min.x, from_node.boxes[2].min.x), parent_box);
					node.set_x_slab(0, std::min(from_node.boxes[1].min.x, from_node.boxes[3].min.x), parent_box);
					node.set_z_slab(0, std::max(from_node.boxes[0].max.z, from_node.boxes[1].max.z), parent_box);
					node.set_z_slab(1, std::min(from_node.boxes[2].min.z, from_node.boxes[3].min.z), parent_box);

			#ifdef Y_SLAB_COMPRESSION
					float y_min = std::min(
									std::min(from_node.boxes[0].min.y, from_node.boxes[1].min.y),
									std::min(from_node.boxes[2].min.y, from_node.boxes[3].min.y)
								);
					node.set_y_min(y_min, parent_box);
					float y_max = std::max(
									std::max(from_node.boxes[0].max.y, from_node.boxes[1].max.y),
									std::max(from_node.boxes[2].max.y, from_node.boxes[3].max.y)
								);
					node.set_y_max(y_max, parent_box);
			#else
					node.set_y_min(0, from_node.boxes[0].min.y, parent_box);
					node.set_y_min(1, from_node.boxes[1].min.y, parent_box);
					node.set_y_min(2, from_node.boxes[2].min.y, parent_box);
					node.set_y_min(3, from_node.boxes[3].min.y, parent_box);
					node.set_y_max(0, from_node.boxes[0].max.y, parent_box);
					node.set_y_max(1, from_node.boxes[1].max.y, parent_box);
					node.set_y_max(2, from_node.boxes[2].max.y, parent_box);
					node.set_y_max(3, from_node.boxes[3].max.y, parent_box);
			#endif

					return node;
				}

				static patch_slab_node copy(const subd::patch_slab_node &from_node) {
					patch_slab_node node;
					uint32_t size = sizeof(node.box_data)/sizeof(node.box_data[0]);
					for (uint32_t i = 0; i < size; ++i)
						node.box_data[i] = from_node.box_data[i];
						
					return node;
				}

				template<typename AABB>
				AABB heterogeneous inline get_box(uint32_t index, AABB parent_box) const {
					//assert(index <= 3);
				#ifdef Y_SLAB_COMPRESSION
					if (index == 0)         return make_aabb<AABB>(parent_box.min.x,      y_min(parent_box), parent_box.min.z,
																   x_slab(0, parent_box), y_max(parent_box), z_slab(0, parent_box));
					else if (index == 1)    return make_aabb<AABB>(x_slab(1, parent_box), y_min(parent_box), parent_box.min.z,
																   parent_box.max.x,      y_max(parent_box), z_slab(0, parent_box));
					else if (index == 2)    return make_aabb<AABB>(parent_box.min.x,      y_min(parent_box), z_slab(1, parent_box),
																   x_slab(0, parent_box), y_max(parent_box), parent_box.max.z);
					else                    return make_aabb<AABB>(x_slab(1, parent_box), y_min(parent_box), z_slab(1, parent_box),
																   parent_box.max.x,      y_max(parent_box), parent_box.max.z);
				#else
					if (index == 0)         return make_aabb<AABB>(parent_box.min.x,      y_min(0, parent_box), parent_box.min.z,
																   x_slab(0, parent_box), y_max(0, parent_box), z_slab(0, parent_box));
					else if (index == 1)    return make_aabb<AABB>(x_slab(1, parent_box), y_min(1, parent_box), parent_box.min.z,
																   parent_box.max.x,      y_max(1, parent_box), z_slab(0, parent_box));
					else if (index == 2)    return make_aabb<AABB>(parent_box.min.x,      y_min(2, parent_box), z_slab(1, parent_box),
																   x_slab(0, parent_box), y_max(2, parent_box), parent_box.max.z);
					else                    return make_aabb<AABB>(x_slab(1, parent_box), y_min(3, parent_box), z_slab(1, parent_box),
																   parent_box.max.x,      y_max(3, parent_box), parent_box.max.z);
				#endif
				}
			};
		#endif


	#endif
#endif

//#endif
}
