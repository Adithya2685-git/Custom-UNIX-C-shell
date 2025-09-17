#ifndef INTRINSICS_H
#define INTRINSICS_H

#include "shell.h"

int is_intrinsic(const char* cmd_name);
void execute_intrinsic(AtomicCommand* atomic, int in_child);
void init_log();
void add_to_log(const char* command);
void log_execute(int index, int in_child);

#endif // INTRINSICS_H