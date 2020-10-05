/*
 * 	Defindes the user-interaction via shell prompt and script file.
 *
 */
#pragma once

#include "libgi/context.h"

#include <iostream>

//! Keep track of when the user changed important values we have to know about in other places.
struct repl_update_checks {
	unsigned cmdid = 0,
			 scene_touched_at = 0,
			 tracer_touched_at = 0,
			 accel_touched_at = 0;
};

//! Call the read-eval-print-loop (can be called multiple times, e.g. for the script and the cin)
void repl(std::istream &infile, render_context &rc, repl_update_checks &uc);

