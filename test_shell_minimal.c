/* test_shell_minimal.c - Test minimal pour le Shell AI-OS */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAX_COMMAND_LENGTH 512
#define MAX_ARGS 32

int parse_command(const char* input, char* command, char args[MAX_ARGS][128], int* arg_count) {
    *arg_count = 0;
    int cmd_len = 0;
    int in_word = 0;
    int in_quotes = 0;
    int arg_idx = 0;
    int char_idx = 0;

    // Extraire la commande
    int i = 0;
    while (input[i] == ' ' || input[i] == '\t') i++; // Skip whitespace

    while (input[i] != '\0' && input[i] != ' ' && input[i] != '\t') {
        command[cmd_len++] = input[i++];
    }
    command[cmd_len] = '\0';

    // Extraire les arguments
    while (input[i] != '\0' && *arg_count < MAX_ARGS) {
        if (input[i] == ' ' || input[i] == '\t') {
            if (in_word && !in_quotes) {
                args[arg_idx][char_idx] = '\0';
                (*arg_count)++;
                arg_idx++;
                char_idx = 0;
                in_word = 0;
            }
        } else if (input[i] == '"') {
            in_quotes = !in_quotes;
            in_word = 1;
        } else {
            if (!in_word) {
                in_word = 1;
                char_idx = 0;
            }
            args[arg_idx][char_idx++] = input[i];
        }
        i++;
    }

    if (in_word) {
        args[arg_idx][char_idx] = '\0';
        (*arg_count)++;
    }

    return strlen(command) > 0 ? 1 : 0;
}

int main() {
    char command[128];
    char args[MAX_ARGS][128];
    int arg_count;

    const char* input = "echo hello world";
    printf("Parsing: %s\n", input);

    if (parse_command(input, command, args, &arg_count)) {
        printf("Command: %s\n", command);
        printf("Arg count: %d\n", arg_count);
        for (int i = 0; i < arg_count; i++) {
            printf("Arg[%d]: %s\n", i, args[i]);
        }
    } else {
        printf("Parse failed\n");
    }

    return 0;
}
