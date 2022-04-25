#pragma once
#ifndef GL_core_profile
namespace wf::gl {
	enum GL_BUFFER_BINDINGS {
		BIND_RAYS = 0,
		BIND_ISEC = 1,
		BIND_VERT = 2,
		BIND_TRIS = 3,
		BIND_NODE = 4,
		BIND_TIDS = 5,
		BIND_MTLS = 6,
		BIND_FBUF = 7,
		BIND_TEXD = 8,
		BIND_RRNG = 9,
	};
}
#else
#define BIND_RAYS 0
#define BIND_ISEC 1
#define BIND_VERT 2
#define BIND_TRIS 3
#define BIND_NODE 4
#define BIND_TIDS 5
#define BIND_MTLS 6
#define BIND_FBUF 7
#define BIND_TEXD 8
#define BIND_RRNG 9
#endif
