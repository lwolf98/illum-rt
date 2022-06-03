/*
 * 	Defindes the user-interaction via shell prompt and script file.
 *
 */
#pragma once

#include "libgi/context.h"

#include <iostream>

void queue_command(const std::string &command);
void process_command_queue(); // runs until exit command is given
void run_repls();
