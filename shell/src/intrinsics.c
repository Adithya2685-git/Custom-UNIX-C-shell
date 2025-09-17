#include "../include/intrinsics.h"


void merge(char* arr[], int left, int mid, int right) {
    int i, j,k;
    int n1 = mid-left+1;
    int n2 = right - mid;

    // Create temporary arrays
    char** L = malloc(n1 * sizeof(char*));
    char** R = malloc(n2 * sizeof(char*));

    // Copy data to temporary arrays L[] and R[]
    for (i = 0; i < n1; i++)
        L[i]=arr[left+ i];
    for (j= 0; j< n2;j++)
        R[j]= arr[mid+ 1+ j];

    i = 0;
    j = 0; 
    k = left;
    while (i< n1 && j< n2) {
        if (strcmp(L[i],R[j])<= 0) {
            arr[k]= L[i];
            i++;
        } else {
            arr[k]= R[j];
            j++;
        }
        k++;
    }

    while (i< n1) {
        arr[k] = L[i];
        i++;
        k++;
    }

    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }
    free(L);
    free(R);
}


void mergeSort(char* arr[], int left, int right) {
    if(left>= right)
    return;

    int mid= left+(right-left)/2;

    mergeSort(arr,left,mid);
    mergeSort(arr,mid+1,right);

    merge(arr,left,mid,right);
}

// intrinsic functions
void hop(AtomicCommand* atomic);
void reveal(AtomicCommand* atomic);
void log_cmd(AtomicCommand* atomic, int in_child);
void activities(AtomicCommand* atomic);
void ping(AtomicCommand* atomic);
void fg(AtomicCommand* atomic);
void bg(AtomicCommand* atomic);

char* LOG_FILE_PATH;
char** history;
int history_count = 0;

void init_log() {
    LOG_FILE_PATH = malloc(PATH_MAX);
    snprintf(LOG_FILE_PATH, PATH_MAX, "%s/.my_shell_history", getenv("HOME"));
    history = calloc(MAX_HISTORY, sizeof(char*));
    
    FILE* file = fopen(LOG_FILE_PATH, "r");
    if (file) {
        char line[MAX_COMMAND_LEN];
        while(fgets(line, sizeof(line), file) && history_count < MAX_HISTORY) {
            line[strcspn(line, "\n")] = 0;
            history[history_count++] = strdup(line);
        }
        fclose(file);
    }
}

void save_log() {
    FILE* file = fopen(LOG_FILE_PATH, "w");
    if(file) {
        for(int i = 0; i < history_count; i++) {
            fprintf(file, "%s\n", history[i]);
        }
        fclose(file);
    }
}

void add_to_log(const char* command) {
    if (history_count > 0 && strcmp(history[history_count-1], command) == 0) {
        return; // Don't add identical consecutive commands
    }
    if (strncmp(command, "log", 3) == 0) {
        return; // Don't add log command
    }

    if (history_count == MAX_HISTORY) {
        free(history[0]);
        memmove(&history[0], &history[1], (MAX_HISTORY - 1) * sizeof(char*));
        history_count--;
    }
    history[history_count++] = strdup(command);
    save_log();
}

void log_execute(int index, int in_child) {
    // When invoked on the RHS of a pipeline, the current full command line has
    // already been appended to history by the parent shell loop. Exclude it so
    // "log execute N" refers to prior entries and does not recurse on itself.
    int effective_hist_count = history_count - (in_child ? 1 : 0);

    if (index < 1 || index > effective_hist_count) {
        printf("log: Invalid index\n");
        return;
    }

    char* cmd_to_run = history[effective_hist_count - index];
    ShellCommand* sc = parse_input(cmd_to_run);
    if (sc) {
        if (in_child && sc->group_count == 1 && sc->groups[0].atomic_count == 1) {
            AtomicCommand* a = &sc->groups[0].atomics[0];
            execvp(a->name, a->args);
            fprintf(stderr, "Command not found!\n");
            free_shell_command(sc);
            exit(EXIT_FAILURE);
        } else {
            execute_shell_command(sc);
            free_shell_command(sc);
        }
    }
}


const char* intrinsics_list[] = {"hop", "reveal", "log", "activities", "ping", "fg", "bg", "exit"};

int is_intrinsic(const char* cmd_name) {
    for (size_t i = 0; i < sizeof(intrinsics_list) / sizeof(char*); i++) {
        if (strcmp(cmd_name, intrinsics_list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void execute_intrinsic(AtomicCommand* atomic, int in_child) {
    if (strcmp(atomic->name, "hop") == 0) hop(atomic);
    else if (strcmp(atomic->name, "reveal") == 0) reveal(atomic);
    else if (strcmp(atomic->name, "log") == 0) log_cmd(atomic, in_child);
    else if (strcmp(atomic->name, "activities") == 0) activities(atomic);
    else if (strcmp(atomic->name, "ping") == 0) ping(atomic);
    else if (strcmp(atomic->name, "fg") == 0) fg(atomic);
    else if (strcmp(atomic->name, "bg") == 0) bg(atomic);
    else if (strcmp(atomic->name, "exit") == 0) cleanup_and_exit();
}
// --- Intrinsic Implementations ---

void hop(AtomicCommand* atomic) {
    char current_dir[PATH_MAX];
    getcwd(current_dir, PATH_MAX);

    if (atomic->arg_count <= 1) { // hop or hop ~
        strcpy(PREV_WORK_DIR, current_dir);
        chdir(SHELL_HOME_DIR);
        return;
    }

    for (int i = 1; i < atomic->arg_count; i++) {
        getcwd(current_dir, PATH_MAX);
        char* target = atomic->args[i];
        
        if (strcmp(target, "~") == 0) {
             strcpy(PREV_WORK_DIR, current_dir);
             chdir(SHELL_HOME_DIR);
        } else if (strcmp(target, ".") == 0) {
            continue;
        } else if (strcmp(target, "..") == 0) {
            strcpy(PREV_WORK_DIR, current_dir);
            chdir("..");
        } else if (strcmp(target, "-") == 0) {
            if (strlen(PREV_WORK_DIR) == 0) {
                printf("hop: no previous directory\n");
            } else {
                char temp[PATH_MAX];
                strcpy(temp, PREV_WORK_DIR);
                strcpy(PREV_WORK_DIR, current_dir);
                chdir(temp);
            }
        } else {
             strcpy(PREV_WORK_DIR, current_dir);
             if (chdir(target) != 0) {
                 printf("No such directory!\n");
                 chdir(PREV_WORK_DIR); // Revert on failure
             }
        }
    }
}

// Return a dynamically allocated string with the path, or NULL on error.
char* resolve_path_argument(const char* arg) {
    if (arg == NULL || strcmp(arg, "~") == 0) {
        return strdup(SHELL_HOME_DIR);
    }
    if (strcmp(arg, "-") == 0) {
        if (strlen(PREV_WORK_DIR) == 0) {
            printf("No such directory!\n");
            return NULL;
        }
        return strdup(PREV_WORK_DIR);
    }
    // For ., .. , or a regular name, the path is just the argument itself.
    return strdup(arg);
}

void reveal(AtomicCommand* atomic) {
    int show_all = 0, line_by_line = 0;
    char* path_arg = ".";
    int path_args_count = 0;

    // parse flags and the single path argument
    for (int i = 1; i < atomic->arg_count; i++) {
        if (atomic->args[i][0] == '-' && strlen(atomic->args[i]) > 1) {
            for (size_t j = 1; j < strlen(atomic->args[i]); j++) {
                if (atomic->args[i][j] == 'a') show_all = 1;
                else if (atomic->args[i][j] == 'l') line_by_line = 1;
            }
        } else {
            path_arg = atomic->args[i];
            path_args_count++;
        }
    }

    if (path_args_count > 1) {
        printf("reveal: Invalid Syntax!\n");
        return;
    }

    char* target_dir = resolve_path_argument(path_arg);
    if (!target_dir) return;

    DIR* d = opendir(target_dir);
    if (!d) {
        printf("No such directory!\n");
        free(target_dir);
        return;
    }
    
    struct dirent* dir;
    char** entries = NULL;
    int count = 0;

    //  Collect all directory entries
    while((dir = readdir(d)) != NULL) {
        if (!show_all && dir->d_name[0] == '.') continue;
        entries = realloc(entries, (count + 1) * sizeof(char*));
        entries[count++] = strdup(dir->d_name);
    }
    closedir(d);
    free(target_dir);

    if (count > 0) {
        mergeSort(entries,0, count-1);
        
        //Print sorted entries
        for (int i = 0; i < count; i++) {
            printf("%s%s", entries[i], line_by_line ? "\n" : (i == count - 1 ? "" : "\t"));
        }
        printf("\n");

        //Clean memory
        for (int i = 0; i < count; i++) {
            free(entries[i]);
        }
        free(entries);
    }
}


void log_cmd(AtomicCommand* atomic, int in_child) {
    if (atomic->arg_count == 1) { // "log"
        for (int i = 0; i < history_count; i++) {
            printf("%s\n", history[i]);
        }
    } else if (strcmp(atomic->args[1], "purge") == 0) {
        for (int i = 0; i < history_count; i++) {
            free(history[i]);
        }
        history_count = 0;
        save_log(); // Overwrite with empty
    } else if (strcmp(atomic->args[1], "execute") == 0) {
        if (atomic->arg_count > 2) {
            int index = atoi(atomic->args[2]);
            log_execute(index, in_child);
        } else {
            printf("log: execute requires an index\n");
        }
    }
}

void activities(AtomicCommand* atomic) {
    (void)atomic; // Unused
    Job *current = bg_jobs_list;
    if (!current) {
        return;
    }

    while (current) {
        printf("[%d] : %s - %s\n",
               current->pid,
               current->command_name ? current->command_name : "(null)",
               current->status ? current->status : "(null)");
        current = current->next;
    }
}

void ping(AtomicCommand* atomic) {
     if (atomic->arg_count != 3) {
        printf("ping: Invalid Syntax!\n");
        return;
    }
    pid_t pid = atoi(atomic->args[1]);
    int sig = atoi(atomic->args[2]);
    if (pid == 0 || (sig == 0 && strcmp(atomic->args[2], "0") != 0)) {
        printf("ping: Invalid Syntax!\n");
        return;
    }

    if (kill(-pid, sig % 32) == -1) {
        printf("No such process found\n");
    } else {
        printf("Sent signal %d to process with pid %d\n", sig, pid);
    }
}

void fg(AtomicCommand* atomic) {
    if (!g_is_interactive) {
        printf("fg: no job control in non-interactive shell\n");
        return;
    }

    int job_id_to_find = (atomic->arg_count > 1) ? atoi(atomic->args[1]) : (next_job_id - 1);
    Job* job = bg_jobs_list;
    Job* prev = NULL;
    while(job && job->job_id != job_id_to_find) {
        prev = job;
        job = job->next;
    }

    if (!job) {
        printf("fg: No such job\n");
        return;
    }
    
    if(prev) prev->next = job->next;
    else bg_jobs_list = job->next;

    printf("%s\n", job->command_name);
    
    //Give the job's process group control of the terminal
    tcsetpgrp(STDIN_FILENO, job->pid);
    
    if (strcmp(job->status, "Stopped") == 0) {
        kill(-job->pid, SIGCONT);
    }
    
    FOREGROUND_PID = job->pid;
    
    int status;
    waitpid(job->pid, &status, WUNTRACED);
    
    //Take back terminal control for the shell
    tcsetpgrp(STDIN_FILENO, getpgrp());
    FOREGROUND_PID = 0;
    
    if (WIFSTOPPED(status)) {
        free(job->status);
        job->status = strdup("Stopped");
        job->next = bg_jobs_list;
        bg_jobs_list = job;
        printf("\n[%d] Stopped %s\n", job->job_id, job->command_name);
    } else {
        free(job->command_name);
        free(job->status);
        free(job);
    }
}

void bg(AtomicCommand* atomic) {
    int job_id_to_find = (atomic->arg_count > 1) ? atoi(atomic->args[1]) : next_job_id - 1;
    Job* job = bg_jobs_list;
    while(job && job->job_id != job_id_to_find) {
        job = job->next;
    }
     if (!job) {
        printf("No such job\n");
        return;
    }
    if(strcmp(job->status, "Running") == 0) {
        printf("Job already running\n");
        return;
    }
    kill(job->pid, SIGCONT);
    free(job->status);
    job->status = strdup("Running");
    printf("[%d] %s &\n", job->job_id, job->command_name);
}
