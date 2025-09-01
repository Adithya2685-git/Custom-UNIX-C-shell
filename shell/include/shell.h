#ifndef SHELL_H
#define SHELL_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>

#define MAX_COMMAND_LEN 4096
#define MAX_ARGS 128
#define MAX_HISTORY 15

typedef struct AtomicCommand {
    char *name;
    char **args;
    int arg_count;

    // Redirections (support ordered processing: first then last)
    char *first_input_file;         // first '<' target (processed first)
    char *input_file;               // last  '<' target (processed second, overrides)

    char *first_output_file;        // first '>' or '>>' target (processed first, creates/truncates/appends)
    int   first_append_output;      // 1 if first output was '>>', else 0

    char *output_file;              // last  '>' or '>>' target (processed second, final stdout)
    int   append_output;            // 1 if last output was '>>', else 0
} AtomicCommand;

typedef struct CommandGroup {
    AtomicCommand *atomics;
    int atomic_count;
} CommandGroup;

typedef struct ShellCommand {
    CommandGroup *groups;
    int group_count;
    char *separators;
    int is_background;
} ShellCommand;

typedef struct Job {
    pid_t pid;
    int job_id;
    char *command_name;
    char *status;
    struct Job *next;
} Job;

// --- Global Variables
extern char SHELL_HOME_DIR[PATH_MAX];
extern char PREV_WORK_DIR[PATH_MAX];
extern volatile pid_t FOREGROUND_PID;
extern Job *bg_jobs_list;
extern int next_job_id;
extern int g_is_interactive; 
extern pid_t g_shell_pgid; 

// prompt.h
void display_prompt();

// parser.h
ShellCommand* parse_input(char* input);
void free_shell_command(ShellCommand* sc);

// execute.h
void execute_shell_command(ShellCommand* sc);

// intrinsics.h
int is_intrinsic(const char* cmd_name);
void execute_intrinsic(AtomicCommand* atomic, int in_child);
void init_log();
void add_to_log(const char* command);
void log_execute(int index, int in_child);

// signals.h
void setup_signal_handlers();
void check_background_processes();
void cleanup_and_exit();


#endif 