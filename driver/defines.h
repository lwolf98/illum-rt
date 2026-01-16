#pragma once

// Box approximation defines
#define BOX_APPROXIMATION
//#define PROJECTION
//#define KEEP_GEOMETRY

// Compression defines
#define SLAB_COMPRESSION
//#define Y_SLAB_COMPRESSION
#define HALF_SLAB_COMPRESSION
#define QUANTIZATION

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