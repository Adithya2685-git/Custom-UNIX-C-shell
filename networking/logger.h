#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

void log_init(const char* filename);
void log_event(const char* format, ...);
void log_close();

extern FILE* log_file;
extern int logging_enabled;

#endif // LOGGER_H

