#pragma once
#include <stdint.h>

#ifdef __CUDACC__
#define heterogeneous __host__ __device__
#else
#define heterogeneous
#endif

namespace subd {
	/**
	 * sub quad ref:	27 bit (supports up to subd_level 13)
	 * hit box side:	 4 bit
	 * upper/lower tri:	 1 bit
	 */
	class quad_ref {
		uint32_t data_field;

		public:
		heterogeneous quad_ref() : data_field(0) {}
		heterogeneous quad_ref(uint32_t data_field) : data_field(data_field) {}

		heterogeneous uint32_t ref() const { return data_field >> 5; }

		heterogeneous void set_ref(uint32_t ref) {
			// limit input value to 27 bits
			ref &= 0x07FFFFFFu;
			// reset first 27 bits to 0 and write into data_field
			data_field = (data_field & 0x1Fu) | (ref << 5);
		}

		heterogeneous uint32_t hit_side() const { return (data_field & (0xFu << 1)) >> 1; }

		heterogeneous void set_hit_side(uint32_t side) {
			// limit input value to 4 bits
			side &= 0xFu;
			uint32_t reset_mask = ~(0xFu << 1);
			// reset the 4 relevant bits to 0 and write into data_field
			data_field = (data_field & reset_mask) | (side << 1);
		}

		// 1 -> upper tri, 0 -> lower tri
		heterogeneous bool is_upper_tri() const { return data_field & 1; }

		heterogeneous void set_upper_tri(bool upper_tri) {
			// reset last bit to 0 with & and write data with |
			data_field = (data_field & ~1u) | static_cast<uint32_t>(upper_tri);
		}

		// provides the internal data field, can be used to reconstruct this object
		heterogeneous uint32_t internal_data() const { return data_field; }
	};


	// TODO: bring compute_valid_hit function here and make heterogeneous?

}