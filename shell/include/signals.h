#ifndef SIGNALS_H
#define SIGNALS_H

#include "shell.h"

void setup_signal_handlers();
void check_background_processes();
void cleanup_and_exit();

#endif // SIGNALS_H