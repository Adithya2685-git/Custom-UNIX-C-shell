#include "../include/parser.h"
#include <ctype.h>
void free_shell_command(ShellCommand* sc) {
    if (!sc) return;
    for (int i = 0; i < sc->group_count; i++) {
        for (int j = 0; j < sc->groups[i].atomic_count; j++) {
            AtomicCommand* a = &sc->groups[i].atomics[j];
            free(a->name);
            for (int k = 0; k < a->arg_count; k++) {
                free(a->args[k]);
            }
            free(a->args);
            free(a->first_input_file);
            free(a->input_file);
            free(a->first_output_file);
            free(a->output_file);
        }
        free(sc->groups[i].atomics);
    }
    free(sc->groups);
    free(sc->separators);
    free(sc);
}

int is_whitespace_only(const char* str) {
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}


int parse_group(CommandGroup* group, char* group_str) {
    group->atomics = NULL;
    group->atomic_count = 0;
    char* atomic_str_base = group_str;

    while (1) {
        char* next_pipe = strchr(atomic_str_base, '|');
        if (next_pipe) *next_pipe = '\0';

        if (is_whitespace_only(atomic_str_base)) return -1;

        AtomicCommand* resized = realloc(group->atomics, (group->atomic_count + 1) * sizeof(AtomicCommand));
        if (!resized) return -1;
        group->atomics = resized;

        AtomicCommand* atomic = &group->atomics[group->atomic_count];
        memset(atomic, 0, sizeof(AtomicCommand));

        atomic->args = malloc(MAX_ARGS * sizeof(char*));
        if (!atomic->args)
            return -1;

        char* saveptr;
        char* token = strtok_r(atomic_str_base, " \t\n\r", &saveptr);
        while (token) {
            if (strcmp(token, "<") == 0) {
                char* tgt = strtok_r(NULL, " \t\n\r", &saveptr);
                if (tgt) {
                    if (!atomic->first_input_file) {
                        atomic->first_input_file = strdup(tgt);
                        if (!atomic->first_input_file) return -1;
                    }
                    free(atomic->input_file);
                    atomic->input_file = strdup(tgt);
                    if (!atomic->input_file)
                        return -1;
                }
            } else if (strcmp(token, ">") == 0) {
                char* tgt = strtok_r(NULL, " \t\n\r", &saveptr);
                if (tgt) {
                    if (!atomic->first_output_file) {
                        atomic->first_output_file = strdup(tgt);
                        if (!atomic->first_output_file) 
                            return -1;
                        atomic->first_append_output = 0;
                    }
                    free(atomic->output_file);
                    atomic->output_file = strdup(tgt);
                    
                    if (!atomic->output_file) 
                        return -1;

                    atomic->append_output = 0;
                }
            } else if (strcmp(token, ">>") == 0) {
                char* tgt = strtok_r(NULL, " \t\n\r", &saveptr);

                if (tgt) {
                    if (!atomic->first_output_file) {
                        atomic->first_output_file = strdup(tgt);

                        if (!atomic->first_output_file) 
                            return -1;

                        atomic->first_append_output = 1;
                    }
                    free(atomic->output_file);
                    atomic->output_file = strdup(tgt);
                    if (!atomic->output_file) 
                        return -1;

                    atomic->append_output = 1;
                }
            } else {
                if (!atomic->name) {
                    atomic->name = strdup(token);
                    if (!atomic->name) return -1;
                }
                atomic->args[atomic->arg_count] = strdup(token);
                if (!atomic->args[atomic->arg_count]) return -1;
                atomic->arg_count++;
            }
            token = strtok_r(NULL, " \t\n\r", &saveptr);
        }

        atomic->args[atomic->arg_count] = NULL;
        group->atomic_count++;

        if (!next_pipe) break;
        atomic_str_base = next_pipe + 1;
    }
    return 0;
}



ShellCommand* parse_input(char* input) {
    char* original_input = strdup(input);
    ShellCommand* sc = calloc(1, sizeof(ShellCommand));
    
    int len = strlen(original_input);
    while (len > 0 && isspace(original_input[len - 1])) original_input[--len] = '\0';
    if (len > 0 && original_input[len - 1] == '&') {
        sc->is_background = 1;
        original_input[--len] = '\0';
    }
    char* current_pos = original_input;
    while (*current_pos) {
        if (is_whitespace_only(current_pos)) break;

        char* end_pos = current_pos;
        while (*end_pos != '\0' && *end_pos != ';' && *end_pos != '&') 
            end_pos++;
        
        char separator = *end_pos;
        *end_pos = '\0';

        if (is_whitespace_only(current_pos)) {
            fprintf(stderr, "Invalid Syntax!\n");
            free_shell_command(sc);
            free(original_input);
            return NULL;
        }

        sc->groups = realloc(sc->groups, (sc->group_count + 1) * sizeof(CommandGroup));
        if (parse_group(&sc->groups[sc->group_count], current_pos) != 0) {
            fprintf(stderr, "Invalid Syntax!\n");
            free_shell_command(sc);
            free(original_input);
            return NULL;
        }
        
        sc->group_count++;
        current_pos = end_pos + (separator != '\0' ? 1 : 0);

        // check for dangling separators
        if ((separator == ';' || separator == '&') && is_whitespace_only(current_pos)) {
            fprintf(stderr, "Invalid Syntax!\n");
            free_shell_command(sc);
            free(original_input);
            return NULL;
        }

        if (separator != '\0') {
            sc->separators = realloc(sc->separators, sc->group_count * sizeof(char));
            sc->separators[sc->group_count - 1] = separator;
        }
    }

    free(original_input);
    return sc;
}
