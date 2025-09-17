#include "../include/execute.h"
void execute_pipeline(CommandGroup* group, int is_background);

// Build a printable command string from args
// If bg is nonzero, append &
static char* build_command_string(const AtomicCommand* a, int is_background) {
    if (!a || !a->args || a->arg_count <= 0) return NULL;
    size_t len = 0;
    for (int i = 0; i < a->arg_count; i++) {
        len += strlen(a->args[i]) + 1; // space 
    }
    if (is_background) len += 2; // space plus &
    char* buf = malloc(len + 1);
    if (!buf) return NULL;
    buf[0] = '\0';
    for (int i = 0; i < a->arg_count; i++) {
        strcat(buf, a->args[i]);
        if (i < a->arg_count - 1) strcat(buf, " ");
    }
    if (is_background) strcat(buf, " &");
    return buf;
}

void execute_shell_command(ShellCommand* sc) {
    if (!sc || sc->group_count == 0) return;

    for (int i = 0; i < sc->group_count; i++) {
        int is_group_background = 0;
        if (i < sc->group_count - 1 && sc->separators[i] == '&') {
            is_group_background = 1;
        } else if (i == sc->group_count - 1 && sc->is_background) {
            is_group_background = 1;
        }
        
        execute_pipeline(&sc->groups[i], is_group_background);
    }
}

void add_bg_job(pid_t pid, const char* name, const char* status) {
    Job* new_job = malloc(sizeof(Job));
    if (!new_job) {
        return;
    }
    new_job->pid = pid;
    new_job->job_id = next_job_id++;
    new_job->command_name = strdup(name);
    new_job->status = strdup(status);
    new_job->next = bg_jobs_list;
    bg_jobs_list = new_job;
}

void execute_pipeline(CommandGroup* group, int is_background) {
    if (group->atomic_count == 0) return;
    
    AtomicCommand* first_atomic = &group->atomics[0];
    if (group->atomic_count == 1 && !is_background && is_intrinsic(first_atomic->name) &&
        first_atomic->input_file == NULL && first_atomic->output_file == NULL) {
        execute_intrinsic(first_atomic, 0);
        return;
    }

    int num_pipes = group->atomic_count - 1;
    int pipe_fds[2 * num_pipes];
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipe_fds + i * 2) < 0) { perror("pipe"); return; }
    }

    pid_t* pids = malloc(group->atomic_count * sizeof(pid_t));
    if (!pids) {
        for (int i = 0; i < 2 * num_pipes; i++) close(pipe_fds[i]);
        return;
    }

    pid_t pgid = 0;

    for (int i = 0; i < group->atomic_count; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            if (g_is_interactive) {
                pgid = (i == 0) ? getpid() : pgid;
                setpgid(0, pgid);
                if (!is_background) {
                    tcsetpgrp(STDIN_FILENO, pgid);
                }
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
            }

            if (i > 0) dup2(pipe_fds[(i - 1) * 2], STDIN_FILENO);

            AtomicCommand* ac = &group->atomics[i];

            if (ac->first_input_file && (!ac->input_file || strcmp(ac->first_input_file, ac->input_file) != 0)) {
                int fd_check_in = open(ac->first_input_file, O_RDONLY);
                if (fd_check_in < 0) { printf("No such file or directory\n"); exit(EXIT_FAILURE); }
                close(fd_check_in);
            }

            if (ac->input_file) {
                int fd_in = open(ac->input_file, O_RDONLY);
                if (fd_in < 0) { printf("No such file or directory\n"); exit(EXIT_FAILURE); }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            if (i < group->atomic_count - 1) dup2(pipe_fds[i * 2 + 1], STDOUT_FILENO);

            if (ac->first_output_file && (!ac->output_file || strcmp(ac->first_output_file, ac->output_file) != 0)) {
                 int first_flags = O_WRONLY | O_CREAT | (ac->first_append_output ? O_APPEND : O_TRUNC);
                 int fd_first_out = open(ac->first_output_file, first_flags, 0644);
                 if (fd_first_out < 0) { printf("Unable to create file for writing\n"); exit(EXIT_FAILURE); }
                 close(fd_first_out);
            }

            if (ac->output_file) {
                 int flags = O_WRONLY | O_CREAT | (ac->append_output ? O_APPEND : O_TRUNC);
                 int fd_out = open(ac->output_file, flags, 0644);
                 if (fd_out < 0) { printf("Unable to create file for writing\n"); exit(EXIT_FAILURE); }
                 dup2(fd_out, STDOUT_FILENO);
                 close(fd_out);
            }
            for(int j = 0; j < 2 * num_pipes; j++) close(pipe_fds[j]);
            
            if (is_intrinsic(group->atomics[i].name)) {
                execute_intrinsic(&group->atomics[i], 1);
                exit(EXIT_SUCCESS);
            } else {
                execvp(group->atomics[i].name, group->atomics[i].args);
                fprintf(stderr,"Command not found!\n");
                exit(EXIT_FAILURE);
            }
        } else if (pids[i] > 0) {
            if (g_is_interactive) {
                if (i == 0) pgid = pids[i];
                setpgid(pids[i], pgid);
            }
        } else {
            perror("fork");
            free(pids);
            return;
        }
    }

    for(int i = 0; i < 2 * num_pipes; i++) close(pipe_fds[i]);

    if (is_background && g_is_interactive) {
        char* cmdstr = build_command_string(&group->atomics[0], 1);
        add_bg_job(pgid, cmdstr ? cmdstr : group->atomics[0].name, "Running");
        if (cmdstr) free(cmdstr);
        printf("[%d] %d\n", next_job_id-1, pgid);
    } else {
        if (g_is_interactive) {
            FOREGROUND_PID = pgid;
            tcsetpgrp(STDIN_FILENO, pgid);
        }
        for(int i = 0; i < group->atomic_count; i++) {
            int status;
            waitpid(pids[i], &status, WUNTRACED);
            if (WIFSTOPPED(status) && g_is_interactive) {
                add_bg_job(pids[i], group->atomics[i].name, "Stopped");
                printf("\n[%d] Stopped %s\n", next_job_id - 1, group->atomics[i].name);
            }
        }
        if (g_is_interactive) {
            tcsetpgrp(STDIN_FILENO, getpgrp());
            FOREGROUND_PID = 0;
        }
    }

    free(pids);
}
