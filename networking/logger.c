#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "logger.h"

FILE* log_file = NULL;
int logging_enabled = 0;

void log_init(const char* filename) {
    if (getenv("RUDP_LOG") && strcmp(getenv("RUDP_LOG"), "1") == 0) {
        logging_enabled = 1;
        log_file = fopen(filename, "w");
        if (log_file == NULL) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }
    }
}

void log_event(const char* format, ...) {
    if (!logging_enabled || log_file == NULL) {
        return;
    }

    char time_buffer[40];
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;

    strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&curtime));
    fprintf(log_file, "[%s.%06ld] [LOG] ", time_buffer, tv.tv_usec);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file); //logs are written immediately
}

void log_close() {
    if (log_file != NULL) {
        fclose(log_file);
    }
}