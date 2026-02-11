#pragma once

/* Configuration */

// Shade by geometry normal instead of shading normal
#define SHADE_BY_GEOMETRY_NORMAL

// Box mid intersection
#define BOX_MID_INTERSECTION
#define BOX_MID_SUPPORT_BACK_SIDE
//#define BOX_MID_VAR_FLAT
//#define BOX_MID_VAR_CARDBOX
#define BOX_MID_VAR_PROJECTION

// Box approximation defines
#define BOX_APPROXIMATION
//#define PROJECTION
//#define KEEP_GEOMETRY

// Compression defines
//#define SLAB_COMPRESSION
//#define Y_SLAB_COMPRESSION
//#define HALF_SLAB_COMPRESSION
//#define QUANTIZATION

// Debugging
//#define DEBUG_LOCAL_ILLUM_NORMALS_SHADING
#define DEBUG_LOCAL_ILLUM_NORMALS_GEOMETRY
//#define DEBUG_LOCAL_ILLUM_ALBEDO

/* Calculations */

#ifndef BOX_MID_INTERSECTION
	constexpr bool def_intersect_box_mid = false;
#else
	constexpr bool def_intersect_box_mid = true;
#endif

#ifndef BOX_APPROXIMATION
	#define APPROXIMATION_VARIANT 0
#else
	#ifndef PROJECTION
		#define APPROXIMATION_VARIANT 1
	#else
		#define APPROXIMATION_VARIANT 2
	#endif
#endif

#ifndef SLAB_COMPRESSION
	#ifndef QUANTIZATION
		#define COMPRESSION_VARIANT 0
	#else
		#define COMPRESSION_VARIANT 1
	#endif
#else
	#ifndef HALF_SLAB_COMPRESSION
		#ifndef QUANTIZATION
			#ifndef Y_SLAB_COMPRESSION
				#define COMPRESSION_VARIANT 2
			#else
				#define COMPRESSION_VARIANT 3
			#endif
		#else
			#ifndef Y_SLAB_COMPRESSION
				#define COMPRESSION_VARIANT 4
			#else
				#define COMPRESSION_VARIANT 5
			#endif
		#endif
	#else
		#ifndef QUANTIZATION
			#ifndef Y_SLAB_COMPRESSION
				#define COMPRESSION_VARIANT 6
			#else
				#define COMPRESSION_VARIANT 7
			#endif
		#else
			#ifndef Y_SLAB_COMPRESSION
				#define COMPRESSION_VARIANT 8
			#else
				#define COMPRESSION_VARIANT 9
			#endif
		#endif
	#endif
#endif

// Box sides
#define BOX_SIDE_UNDEFINED 0
#define BOX_SIDE_FRONT 1
#define BOX_SIDE_BACK 2
#define BOX_SIDE_SIDE_DOWN 3
#define BOX_SIDE_SIDE_RIGHT 4
#define BOX_SIDE_SIDE_UP 5
#define BOX_SIDE_SIDE_LEFT 6
#define BOX_SIDE_DEBUG 7
