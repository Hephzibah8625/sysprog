#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define die(e) do { fprintf(stderr, "%s\n", e); exit(EXIT_FAILURE); } while (0);


struct cmd {
    const char *name;
    const char **argv;
    int argc;
};

void parse_command(char* line, int line_size, struct cmd* cmd_struct) {
    int command_length = 0;
    while (line[command_length] != ' ') command_length++;
    char* command = (char*)malloc(sizeof(char) * command_length);
}

void parse_line(char* line, int length) {
    // trim line
    int start = 0;
    int end = length - 1;

    while (isspace(line[start])) start++;
    while (isspace(line[end])) end--;

    if (start > end) {
        return;
    }

    line[++end] = ' ';
    end++;
    int size = end - start + 1;

    // tokenize line
    int was_ord = 0;
    int was_doubl = 0;
    char prev;
    char* token = (char*)malloc(0 * sizeof(char));
    int token_size = 0;

    for (int i = start; i <= end; i++) {
        char c = line[i];
        if (prev == ' ' && c == '\'' && was_ord == 0) {
            was_ord = 1;
        } else if (c == '\'' && was_ord == 1) {
            printf("%s\n", token);
            for (int j = 0; j < token_size; j++) token[j] = 0;
            token = (char*)realloc(token, 0 * sizeof(char));
            token_size = 0;
            was_ord = 0;
        } else if (prev == ' ' && c == '"' && was_doubl == 0) {
            was_doubl = 1;
        } else if (c == '"' && was_doubl == 1) {
            printf("%s\n", token);
            for (int j = 0; j < token_size; j++) token[j] = 0;
            token = (char*)realloc(token, 0 * sizeof(char));
            token_size = 0;
            was_doubl = 0;
        } else if (prev == '\\' && c == ' ' && was_ord == 0 && was_doubl == 0) {
            token_size++;
            token = (char*)realloc(token, token_size * sizeof(char));
            token[token_size - 1] = ' ';
        } else if (c == ' ' && was_ord == 0 && was_doubl == 0) {
            printf("%s\n", token);
            for (int j = 0; j < token_size; j++) token[j] = 0;
            token = (char*)realloc(token, 0 * sizeof(char));
            token_size = 0;
        } else {
            token_size++;
            token = (char*)realloc(token, token_size * sizeof(char));
            token[token_size - 1] = c;
        }
        prev = c;
    }
    free(token);   
}

int main() {
    while (1) {
        printf("$ ");

        int length = 0;
        char* buffer = (char*)malloc(sizeof(char) * 1);
        char prev;
        struct cmd command;
        while (1) {
            char c = getchar();
            if (prev == '\\') {
                if (c == '\n') {
                    continue;
                    prev = '1';
                }
                else {
                    buffer[length] = prev;
                    length++;
                    buffer = (char*)realloc(buffer, sizeof(char) * length);
                    buffer[length] = c;
                    length++;
                    buffer = (char*)realloc(buffer, sizeof(char) * length);
                    prev = c;
                }
            } else {
                if (c == '\n') {
                    break;
                } else {
                    buffer[length] = c;
                    length++;
                    buffer = (char*)realloc(buffer, sizeof(char) * length);
                    prev = c;
                }
            }
        }
        buffer[length] = 0;

        if (strcmp("exit", buffer) == 0) {
            break;
        }
        parse_line(buffer, length);
        free(buffer);
    }
    return 0;
}