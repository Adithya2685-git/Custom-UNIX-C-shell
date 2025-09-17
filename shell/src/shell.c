#include "../include/shell.h"
#include <ctype.h>
char SHELL_HOME_DIR[PATH_MAX];
char PREV_WORK_DIR[PATH_MAX] = "";
volatile pid_t FOREGROUND_PID = 0;
Job* bg_jobs_list = NULL;
int next_job_id = 1;
int g_is_interactive = 0; 
pid_t g_shell_pgid = 0;

void init_shell() {
    getcwd(SHELL_HOME_DIR, PATH_MAX);
    if (g_is_interactive) {
        setup_signal_handlers();
    }
    init_log();
}

int main() {
    g_is_interactive = isatty(STDIN_FILENO);
    init_shell();
    char input_line[4096];
    while (1) {
        if (g_is_interactive) {
            check_background_processes();
            display_prompt();
        }

        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            break; // EOF or error ctrl+d
        }

        size_t nread = strlen(input_line);

        if (g_is_interactive) {
            check_background_processes();
        }

        if (nread > 0) {
            if (input_line[nread - 1] == '\n') {
                input_line[nread - 1] = '\0';
                nread--;
            }

            int is_whitespace = 1;
            for (size_t i = 0; i < nread; i++) {
                if (!isspace((unsigned char)input_line[i])) {
                    is_whitespace = 0;
                    break;
                }
            }

            if (!is_whitespace) {
                add_to_log(input_line);
                ShellCommand* sc = parse_input(input_line);
                if (sc) {
                    execute_shell_command(sc);
                    free_shell_command(sc);
                }
            }
        }
    }

    if (g_is_interactive) {
        printf("logout\n");
    }
    
    return 0;
}
