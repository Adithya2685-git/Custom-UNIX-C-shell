#ifndef PARSER_H
#define PARSER_H

#include "shell.h"

ShellCommand* parse_input(char* input);
void free_shell_command(ShellCommand* sc);

#endif // PARSER_H