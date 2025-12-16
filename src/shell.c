#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

int main() {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        // Prompt
        printf("myshell> ");
        fflush(stdout);

        // Read input
        if (fgets(line, MAX_LINE, stdin) == NULL) {
            printf("\n");
            break;
        }

        // Remove newline
        line[strcspn(line, "\n")] = '\0';

        // Exit command
        if (strcmp(line, "exit") == 0) {
            break;
        }

        // Tokenize
        int argc = 0;
        char *token = strtok(line, " ");

        while (token != NULL && argc < MAX_ARGS - 1) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }

        args[argc] = NULL;

        // Ignore empty input
        if (args[0] == NULL) {
            continue;
        }

        // Fork
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            continue;
        }

        if (pid == 0) {
            // Child
            execvp(args[0], args);
            perror("exec failed");
            return 1;
        } else {
            // Parent
            wait(NULL);
        }
    }

    return 0;
}
