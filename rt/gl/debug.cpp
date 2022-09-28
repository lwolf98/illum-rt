#include "base.h"

#include <string>

using namespace std;

void debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    // get source of error
    string src;
    switch (source){
    case GL_DEBUG_SOURCE_API:
        src = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        src = "WINDOW_SYSTEM"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        src = "SHADER_COMPILER"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        src = "THIRD_PARTY"; break;
    case GL_DEBUG_SOURCE_APPLICATION:
        src = "APPLICATION"; break;
    case GL_DEBUG_SOURCE_OTHER:
        src = "OTHER"; break;
    }
    // get type of error
    string typ;
    switch (type){
    case GL_DEBUG_TYPE_ERROR:
        typ = "ERROR"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typ = "DEPRECATED_BEHAVIOR"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typ = "UNDEFINED_BEHAVIOR"; break;
    case GL_DEBUG_TYPE_PORTABILITY:
        typ = "PORTABILITY"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        typ = "PERFORMANCE"; break;
    case GL_DEBUG_TYPE_OTHER:
        typ = "OTHER"; break;
    case GL_DEBUG_TYPE_MARKER:
        typ = "MARKER"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        typ = "PUSH_GROUP"; break;
    case GL_DEBUG_TYPE_POP_GROUP:
        typ = "POP_GROUP"; break;
    }
    // get severity
    string sev;
    switch (severity) {
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        sev = "NOTIFICATION"; break;
    case GL_DEBUG_SEVERITY_LOW:
        sev = "LOW"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        sev = "MEDIUM"; break;
    case GL_DEBUG_SEVERITY_HIGH:
        sev = "HIGH"; break;
    }
    fprintf(stderr, "GL_DEBUG: Severity: %s, Source: %s, Type: %s.\nMessage: %s\n", sev.c_str(), src.c_str(), typ.c_str(), message);
}

namespace wf::gl {

	void enable_gl_debug_output() {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(debug_callback, 0);
		disable_gl_notifications();
	}

	void disable_gl_debug_output() {
		glDisable(GL_DEBUG_OUTPUT);
	}

	void enable_gl_notifications() {
		glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DEBUG_SEVERITY_NOTIFICATION, 0, 0, GL_TRUE);
	}

	void disable_gl_notifications() {
		glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DEBUG_SEVERITY_NOTIFICATION, 0, 0, GL_FALSE);
	}

}
