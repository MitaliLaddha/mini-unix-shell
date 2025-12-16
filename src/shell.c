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

        int argc = 0;
        char *token = strtok(line, " ");

        while (token != NULL && argc < MAX_ARGS - 1) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        if (args[0] == NULL) {
            continue;
        }

        // 🔹 Detect background execution
        int background = 0;
        if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
            background = 1;
            args[argc - 1] = NULL; // remove "&"
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            continue;
        }

        if (pid == 0) {
            execvp(args[0], args);
            perror("exec failed");
            return 1;
        } else {
            if (!background) {
                wait(NULL);
            } else {
                printf("[background pid %d]\n", pid);
            }
        }
    }

    return 0;
}
