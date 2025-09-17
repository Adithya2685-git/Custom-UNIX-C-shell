#include "../include/prompt.h"

void display_prompt() {
    char hostname[HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char* username = NULL;

    username = getenv("USER");

    if (username == NULL) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            username = pw->pw_name;
        } else {
            username = "user";
        }
    }
    
    if (gethostname(hostname, HOST_NAME_MAX) != 0) {
        perror("gethostname");
        strcpy(hostname, "system");
    }

    if (getcwd(cwd, PATH_MAX) == NULL) {
        perror("getcwd");
        return;
    }
    // Replace home directory with '~'
    if (strncmp(cwd, SHELL_HOME_DIR, strlen(SHELL_HOME_DIR)) == 0) {
        char temp[PATH_MAX];
        snprintf(temp, PATH_MAX, "~%s", cwd + strlen(SHELL_HOME_DIR));
        strcpy(cwd, temp);
    }
    printf("<%s@%s:%s> ", username, hostname, cwd);
    fflush(stdout);
}