#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

// Reap background children to avoid zombies
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Ignore Ctrl+C in the shell itself
void handle_sigint(int sig) {
    // Do nothing
}

int main() {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    // Register signal handlers
    signal(SIGCHLD, handle_sigchld);
    signal(SIGINT, handle_sigint);

    while (1) {
        // Prompt
        printf("myshell> ");
        fflush(stdout);

        // Read input
        if (fgets(line, MAX_LINE, stdin) == NULL) {
            // Ctrl+D (EOF)
            printf("\n");
            break;
        }

        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // Exit command
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
        args[argc] = NULL;

        // Ignore empty input
        if (args[0] == NULL) {
            continue;
        }

        // Check for background execution
        int background = 0;
        if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
            background = 1;
            args[argc - 1] = NULL; // remove &
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            continue;
        }

        if (pid == 0) {
            // Child: restore default Ctrl+C behavior
            signal(SIGINT, SIG_DFL);

            execvp(args[0], args);
            perror("exec failed");
            return 1;
        } else {
            // Parent
            if (!background) {
                wait(NULL);
            } else {
                printf("[background pid %d]\n", pid);
            }
        }
    }

    return 0;
}
