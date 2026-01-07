#include "subdivision.h"

using namespace glm;

namespace subd {
#ifdef QUANTIZATION
	static constexpr std::array<float,8> thresholds_xz = { 0.00f, 0.40f, 0.48f, 0.49f, 0.50f, 0.51f, 0.52f, 0.60f };
	static constexpr std::array<float,4> thresholds_y = { 0.00f, 0.25f, 0.50f, 0.75f };
	static char quantize_xz(float val) {
		char res = 0;
		for (uint32_t i = 0; i < thresholds_xz.size(); ++i) {
			if (thresholds_xz[i] <= val) res = i;
			else break;
		}
		return res;
	}
	static char quantize_y(float val) {
		char res = 0;
		for (uint32_t i = 0; i < thresholds_y.size(); ++i) {
			if (thresholds_y[i] <= val) res = i;
			else break;
		}
		return res;
	}
	static float dequantize_xz(char val) {
		return thresholds_xz[val];
	}
	static float dequantize_y(char val) {
		return thresholds_y[val];
	}

	inline void set_012(char &field, char val) { field = (field & 0b00011111 | val << 5); }
	inline void set_345(char &field, char val) { field = (field & 0b11100011 | val << 2); }
	inline void set_67 (char &field, char val) { field = (field & 0b11111100 | val); }

	inline char get_012(const char &field) { return ((field & 0b11100000) >> 5); }
	inline char get_345(const char &field) { return ((field & 0b00011100) >> 2); }
	inline char get_67 (const char &field) { return (field & 0b00000011); }
	
	#ifndef Y_SLAB_COMPRESSION
	inline void set_01 (char &field, char val) { field = (field & 0b00111111 | val << 6); }
	inline void set_23 (char &field, char val) { field = (field & 0b11001111 | val << 4); }
	inline void set_45 (char &field, char val) { field = (field & 0b11110011 | val << 2); }

	inline char get_01 (const char &field) { return ((field & 0b11000000) >> 6); }
	inline char get_23 (const char &field) { return ((field & 0b00110000) >> 4); }
	inline char get_45 (const char &field) { return ((field & 0b00001100) >> 2); }
	#endif
#endif

#ifdef SLAB_COMPRESSION
	#ifndef HALF_SLAB_COMPRESSION
		#ifndef QUANTIZATION
			patch_slab_node patch_slab_node::from(const patch_node &from_node) {
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

		aabb patch_slab_node::get_box(uint32_t index) const {
			assert(index <= 3);
			#ifdef Y_SLAB_COMPRESSION
				if (index == 0)			return { vec3(x_slabs[0], y_min, z_slabs[0]),
												 vec3(x_slabs[1], y_max, z_slabs[1]) };
				else if (index == 1)	return { vec3(x_slabs[2], y_min, z_slabs[0]),
												 vec3(x_slabs[3], y_max, z_slabs[1]) };
				else if (index == 2)	return { vec3(x_slabs[0], y_min, z_slabs[2]),
												 vec3(x_slabs[1], y_max, z_slabs[3]) };
				else					return { vec3(x_slabs[2], y_min, z_slabs[2]),
												 vec3(x_slabs[3], y_max, z_slabs[3]) };
			#else
				if (index == 0)			return { vec3(x_slabs[0], y_min[0], z_slabs[0]),
												 vec3(x_slabs[1], y_max[0], z_slabs[1]) };
				else if (index == 1)	return { vec3(x_slabs[2], y_min[1], z_slabs[0]),
												 vec3(x_slabs[3], y_max[1], z_slabs[1]) };
				else if (index == 2)	return { vec3(x_slabs[0], y_min[2], z_slabs[2]),
												 vec3(x_slabs[1], y_max[2], z_slabs[3]) };
				else					return { vec3(x_slabs[2], y_min[3], z_slabs[2]),
												 vec3(x_slabs[3], y_max[3], z_slabs[3]) };
			#endif
		}
		#else
			void patch_slab_node::set_x_slab(uint32_t index, float val, const aabb &parent_box) {
				double x_f = 1.0 / (parent_box.max.x - parent_box.min.x);
				if (!std::isfinite(x_f)) x_f = FLT_MIN;
				switch (index) {
					case 0: set_012(box_data[0], quantize_xz((val - parent_box.min.x) * x_f));
					case 1: set_345(box_data[0], quantize_xz((parent_box.min.x - val) * x_f));
					case 2: set_012(box_data[1], quantize_xz((val - parent_box.min.x) * x_f));
					case 3: set_345(box_data[1], quantize_xz((parent_box.min.x - val) * x_f));
				}
			}
			void patch_slab_node::set_z_slab(uint32_t index, float val, const aabb &parent_box) {
				double z_f = 1.0 / (parent_box.max.z - parent_box.min.z);
				if (!std::isfinite(z_f)) z_f = FLT_MIN;
				switch (index) {
					case 0: set_012(box_data[2], quantize_xz((val - parent_box.min.z) * z_f));
					case 1: set_345(box_data[2], quantize_xz((parent_box.min.z - val) * z_f));
					case 2: set_012(box_data[3], quantize_xz((val - parent_box.min.z) * z_f));
					case 3: set_345(box_data[3], quantize_xz((parent_box.min.z - val) * z_f));
				}
			}
			void patch_slab_node::set_y_min(uint32_t index, float val, const aabb &parent_box) {
				double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
				char q_val = quantize_xz(val - parent_box.min.y) * y_f;
				switch (index) {
					case 0: set_67(box_data[0], q_val);
					case 1: set_67(box_data[1], q_val);
					case 2: set_67(box_data[2], q_val);
					case 3: set_67(box_data[3], q_val);
				}
			}
			void patch_slab_node::set_y_max(uint32_t index, float val, const aabb &parent_box) {
				double y_f = 1.0 / (parent_box.max.y - parent_box.min.y);
				char q_val = quantize_xz(parent_box.max.y - val) * y_f;
				switch (index) {
					case 0: set_01(box_data[4], q_val);
					case 1: set_23(box_data[4], q_val);
					case 2: set_45(box_data[4], q_val);
					case 3: set_67(box_data[4], q_val);
				}
			}

			float patch_slab_node::x_slab(uint32_t index, const aabb &parent_box) const {
				float dim = parent_box.max.x - parent_box.min.x;
				switch (index) {
					case 0: return dequantize_xz(get_012(box_data[0]) * dim + parent_box.min.x);
					case 1: return dequantize_xz((1.f - get_345(box_data[0])) * dim + parent_box.min.x); //REVIEW: parent_box.min here correct??
					case 2: return dequantize_xz(get_012(box_data[1]) * dim + parent_box.min.x);
					case 3: return dequantize_xz((1.f - get_345(box_data[1])) * dim + parent_box.min.x);
				}
				assert(false);
			}
			float patch_slab_node::z_slab(uint32_t index, const aabb &parent_box) const {
				float dim = parent_box.max.z - parent_box.min.z;
				switch (index) {
					case 0: return dequantize_xz(get_012(box_data[2]) * dim + parent_box.min.z);
					case 1: return dequantize_xz((1.f - get_345(box_data[2])) * dim + parent_box.min.z); //REVIEW: parent_box.min here correct??
					case 2: return dequantize_xz(get_012(box_data[3]) * dim + parent_box.min.z);
					case 3: return dequantize_xz((1.f - get_345(box_data[3])) * dim + parent_box.min.z);
				}
				assert(false);
			}
			float patch_slab_node::y_min(uint32_t index, const aabb &parent_box) const {
				float dim = parent_box.max.y - parent_box.min.y;
				switch (index) {
					case 0: return dequantize_y(get_67(box_data[0] * dim + parent_box.min.y));
					case 1: return dequantize_y(get_67(box_data[1] * dim + parent_box.min.y));
					case 2: return dequantize_y(get_67(box_data[2] * dim + parent_box.min.y));
					case 3: return dequantize_y(get_67(box_data[3] * dim + parent_box.min.y));
				}
				assert(false);
			}
			float patch_slab_node::y_max(uint32_t index, const aabb &parent_box) const {
				float dim = parent_box.max.y - parent_box.min.y;
				switch (index) {
					case 0: return dequantize_y((1.f - get_01(box_data[4]) * dim + parent_box.min.y)); //REVIEW: parent_box.min here correct??
					case 1: return dequantize_y((1.f - get_23(box_data[4]) * dim + parent_box.min.y));
					case 2: return dequantize_y((1.f - get_45(box_data[4]) * dim + parent_box.min.y));
					case 3: return dequantize_y((1.f - get_67(box_data[4]) * dim + parent_box.min.y));
				}
				assert(false);
			}

			patch_slab_node patch_slab_node::from(const patch_node &from_node, const aabb &parent_box) {
				patch_slab_node node;
				node.set_x_slab(0, std::min(from_node.boxes[0].min.x, from_node.boxes[2].min.x), parent_box);
				node.set_x_slab(1, std::max(from_node.boxes[0].max.x, from_node.boxes[2].max.x), parent_box);
				node.set_x_slab(2, std::min(from_node.boxes[1].min.x, from_node.boxes[3].min.x), parent_box);
				node.set_x_slab(3, std::max(from_node.boxes[1].max.x, from_node.boxes[3].max.x), parent_box);
				node.set_z_slab(0, std::min(from_node.boxes[0].min.z, from_node.boxes[1].min.z), parent_box);
				node.set_z_slab(1, std::max(from_node.boxes[0].max.z, from_node.boxes[1].max.z), parent_box);
				node.set_z_slab(2, std::min(from_node.boxes[2].min.z, from_node.boxes[3].min.z), parent_box);
				node.set_z_slab(3, std::max(from_node.boxes[2].max.z, from_node.boxes[3].max.z), parent_box);

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
			
			aabb patch_slab_node::get_box(uint32_t index, const aabb &parent_box) const {
				assert(index <= 3);
			#ifdef Y_SLAB_COMPRESSION
				if (index == 0)			return { vec3(x_slabs[0], y_min, z_slabs[0]),
												 vec3(x_slabs[1], y_max, z_slabs[1]) };
				else if (index == 1)	return { vec3(x_slabs[2], y_min, z_slabs[0]),
												 vec3(x_slabs[3], y_max, z_slabs[1]) };
				else if (index == 2)	return { vec3(x_slabs[0], y_min, z_slabs[2]),
												 vec3(x_slabs[1], y_max, z_slabs[3]) };
				else					return { vec3(x_slabs[2], y_min, z_slabs[2]),
												 vec3(x_slabs[3], y_max, z_slabs[3]) };
			#else
				if (index == 0)			return { vec3(x_slab(0, parent_box), y_min(0, parent_box), z_slab(0, parent_box)),
												 vec3(x_slab(1, parent_box), y_max(0, parent_box), z_slab(1, parent_box)) };
				else if (index == 1)	return { vec3(x_slab(2, parent_box), y_min(1, parent_box), z_slab(0, parent_box)),
												 vec3(x_slab(3, parent_box), y_max(1, parent_box), z_slab(1, parent_box)) };
				else if (index == 2)	return { vec3(x_slab(0, parent_box), y_min(2, parent_box), z_slab(2, parent_box)),
												 vec3(x_slab(1, parent_box), y_max(2, parent_box), z_slab(3, parent_box)) };
				else					return { vec3(x_slab(2, parent_box), y_min(3, parent_box), z_slab(2, parent_box)),
												 vec3(x_slab(3, parent_box), y_max(3, parent_box), z_slab(3, parent_box)) };
			#endif
			}
		#endif
	#else
		#ifndef QUANTIZATION
			patch_slab_node patch_slab_node::from(const patch_node &from_node) {
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

			aabb patch_slab_node::get_box(uint32_t index, aabb parent_box) const {
				assert(index <= 3);
			#ifdef Y_SLAB_COMPRESSION
				if (index == 0)			return { vec3(parent_box.min.x, y_min, parent_box.min.z),
												 vec3(x_slabs[1],	   y_max, z_slabs[1]	  ) };
				else if (index == 1)	return { vec3(x_slabs[2],	   y_min, parent_box.min.z),
												 vec3(parent_box.max.x, y_max, z_slabs[1]	  ) };
				else if (index == 2)	return { vec3(parent_box.min.x, y_min, z_slabs[2]	  ),
												 vec3(x_slabs[1],	   y_max, parent_box.max.z) };
				else					return { vec3(x_slabs[2],	   y_min, z_slabs[2]	  ),
												 vec3(parent_box.max.x, y_max, parent_box.max.z) };
			#else
				if (index == 0)			return { vec3(parent_box.min.x, y_min[0], parent_box.min.z),
												 vec3(x_slabs[0],	   y_max[0], z_slabs[0]	  ) };
				else if (index == 1)	return { vec3(x_slabs[1],	   y_min[1], parent_box.min.z),
												 vec3(parent_box.max.x, y_max[1], z_slabs[0]	  ) };
				else if (index == 2)	return { vec3(parent_box.min.x, y_min[2], z_slabs[1]	  ),
												 vec3(x_slabs[0],	   y_max[2], parent_box.max.z) };
				else					return { vec3(x_slabs[1],	   y_min[3], z_slabs[1]	  ),
												 vec3(parent_box.max.x, y_max[3], parent_box.max.z) };
			#endif
			}
			//TODO/REVIEW: required?
			/*aabb get_box(uint32_t box_index, subd_subpatch subpatch, uint32_t node_id)const {
				return subpatch.box_from_node(node_id, box_index);
			}*/
		#else
			patch_slab_node patch_slab_node::from(const patch_node &from_node) {

			}

			aabb patch_slab_node::get_box(uint32_t index, aabb parent_box) const {

			}
		#endif
	#endif
#endif
}