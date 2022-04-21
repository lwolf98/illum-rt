#pragma once

#include "libgi/wavefront-rt.h"

namespace wf::gl {
	/*! \brief OpenGL Platform for Ray Tracing
	 * 	
	 * 	A non-optimized GL implementation of RTGI's interface, primarily as proof of concept and documentation for
	 * 	more advanced GPU (or CPU/SIMD) driven implementations. Well, and then it got a little out of hand. However,
	 * 	for a very simple use case see ../cpu/, and consider this wavefront implementation a (at least comparatively)
	 * 	approachable implementation of a more complex use case.
	 *
	 */
	class platform : public wf::platform {
	public:
		platform(const std::vector<std::string> &args);
		bool interprete(const std::string &command, std::istringstream &in) override;
	};
}
