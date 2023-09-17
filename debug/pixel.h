#pragma once

#include "debug/obj_writer.h"

extern int debug_pixel_x;
extern int debug_pixel_y;
extern bool debug_pixel_single_run_pending;

inline bool debug_pixel(int x, int y) {
	if (debug_pixel_x == x && debug_pixel_y == y && debug_pixel_single_run_pending) {
		debug_pixel_single_run_pending = false;
		return true;
	}
	return false;
}

#define def_obj_debug \
	static thread_local bool debug = false; \
	static obj::obj_writer *ow = nullptr;

#define start_obj_debug(X,Y,N) \
	if (debug_pixel(X, Y)) { \
		debug = true; \
		ow = new obj::obj_writer(N); \
	} \
	else debug = false;

#define finalize_obj_debug { if (debug) { delete ow; ow = nullptr; } }
