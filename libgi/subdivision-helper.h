#include <stdint.h>

namespace subd {
	/**
	 * sub quad ref:	27 bit (supports up to subd_level 13)
	 * arbitrary data:	 4 bit
	 * upper/lower tri:	 1 bit
	 */
	class quad_ref {
		uint32_t data_field;

		public:
		quad_ref() : data_field(0) {}

		uint32_t ref() const { return data_field >> 5; }

		void set_ref(uint32_t ref) {
			// limit input value to 27 bits
			ref &= 0x07FFFFFFu;
			// reset first 27 bits to 0 and write into data_field
			data_field = (data_field & 0x1Fu) | (ref << 5);
		}

		uint32_t data() const { return (data_field & (0xFu << 1)) >> 1; }

		void set_data(uint32_t data) {
			// limit input value to 4 bits
			data &= 0xFu;
			uint32_t reset_mask = ~(0xFu << 1);
			// reset the 4 relevant bits to 0 and write into data_field
			data_field = (data_field & reset_mask) | (data << 1);
		}

		// 1 -> upper tri, 0 -> lower tri
		bool is_upper_tri() const { return data_field & 1; }

		void set_upper_tri(bool upper_tri) {
			// reset last bit to 0 with & and write data with |
			data_field = (data_field & ~1u) | static_cast<uint32_t>(upper_tri);
		}
	};
}