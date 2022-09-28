#pragma once
#ifndef GL_core_profile
namespace wf::gl {
	enum GL_BUFFER_BINDINGS {
		BIND_VERT = 0,
		BIND_TRIS = 1,
		BIND_NODE = 2,
		BIND_TIDS = 3,
		BIND_MTLS = 4,
		BIND_TEXD = 5,
		BIND_RRNG = 6,
		BIND_PRFB = 7,	// note: magic number used in preview.cpp
	};
}
#else
#define BIND_VERT 0
#define BIND_TRIS 1
#define BIND_NODE 2
#define BIND_TIDS 3
#define BIND_MTLS 4
#define BIND_TEXD 5
#define BIND_RRNG 6
#define BIND_PRFB 7
#endif
