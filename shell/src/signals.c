#include "../include/signals.h"
void sigint_handler(int signo) {
    (void)signo;
    if (FOREGROUND_PID > 0) {
        // Send SIGINT to the entire foreground process group.
        kill(-FOREGROUND_PID, SIGINT);
    }
    write(STDOUT_FILENO, "\n", 1);
}

void sigtstp_handler(int signo) {
    (void)signo;
    if (FOREGROUND_PID > 0) {
        // Send SIGTSTP to the entire foreground process group.
        kill(-FOREGROUND_PID, SIGTSTP);
    }
}

// reap and print completion notifications before the next command.
void sigchld_handler(int signo) {
    (void)signo;
}

void setup_signal_handlers() {
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

void check_background_processes() {
    int status;
    pid_t pid;

    // Reap any child that changed state (exit/stop/continue) without blocking
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        // Find the job node matching pid
        Job* prev = NULL;
        Job* cur = bg_jobs_list;
        while (cur && cur->pid != pid) {
            prev = cur;
            cur = cur->next;
        }
        if (!cur) {
            // foreground child Skip.
            continue;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // Finished.  Free Linkedlist

            int commandname_length= strlen(cur->command_name);
            char commandname[commandname_length];
            strcpy(commandname,cur->command_name);
            
            if(commandname_length>=1)
            commandname[commandname_length-1]='\0';

            printf("%swith pid %d exited normally\n",
                   cur->command_name ? commandname : "(null)",
                   cur->pid);
            Job* next = cur->next;
            if (prev) prev->next = next;
            else bg_jobs_list = next;
            free(cur->command_name);
            free(cur->status);
            free(cur);
        } else if (WIFSTOPPED(status)) {
            if (!cur->status || strcmp(cur->status, "Stopped") != 0) {
                free(cur->status);
                cur->status = strdup("Stopped");
            }
        } else if (WIFCONTINUED(status)) {
            if (!cur->status || strcmp(cur->status, "Running") != 0) {
                free(cur->status);
                cur->status = strdup("Running");
            }
        }
    }
}

void cleanup_and_exit() {
    // Send SIGKILL to all children
    Job* current = bg_jobs_list;
    while(current) {
        kill(current->pid, SIGKILL);
        current = current->next;
    }
    printf("logout\n");
    exit(0);
}
