#pragma once

#include "libgi/context.h"

#include <iostream>

struct repl_update_checks {
	unsigned cmdid = 0,
			 scene_touched_at = 0,
			 tracer_touched_at = 0,
			 accel_touched_at = 0;
};

void repl(std::istream &infile, render_context &rc, repl_update_checks &uc);

