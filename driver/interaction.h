/*
 * 	Defindes the user-interaction via shell prompt and script file.
 *
 */
#pragma once

#include "libgi/context.h"

#include <iostream>

enum enqueue_mode { keep_prev_same_commands, remove_prev_same_commands };

void queue_command(const std::string &command, enqueue_mode mode = keep_prev_same_commands);
void process_command_queue(); // runs until exit command is given
void run_repls();