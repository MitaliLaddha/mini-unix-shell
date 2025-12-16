#include <stdio.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

int main() {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        printf("myshell> ");
        fflush(stdout);

        if (fgets(line, MAX_LINE, stdin) == NULL) {
            printf("\n");
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "exit") == 0) {
            break;
        }

        // Tokenize input
        int argc = 0;
        char *token = strtok(line, " ");

        while (token != NULL && argc < MAX_ARGS - 1) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }

        args[argc] = NULL;  // VERY IMPORTANT

        // Debug: print tokens
        for (int i = 0; args[i] != NULL; i++) {
            printf("arg[%d] = %s\n", i, args[i]);
        }
    }

    return 0;
}
