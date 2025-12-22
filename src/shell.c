#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_sigint(int sig) {
    // Ignore Ctrl+C in shell
}

int main() {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    signal(SIGCHLD, handle_sigchld);
    signal(SIGINT, handle_sigint);

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

        // Background execution
        int background = 0;
        if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
            background = 1;
            args[argc - 1] = NULL;
            argc--;
        }

        // Redirection variables
        char *input_file = NULL;
        char *output_file = NULL;

        // Scan args for < or >
        for (int i = 0; i < argc; i++) {
            if (strcmp(args[i], "<") == 0) {
                input_file = args[i + 1];
                args[i] = NULL;
                break;
            }
            if (strcmp(args[i], ">") == 0) {
                output_file = args[i + 1];
                args[i] = NULL;
                break;
            }
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            continue;
        }

        if (pid == 0) {
            // Child: restore default Ctrl+C
            signal(SIGINT, SIG_DFL);

            // Input redirection
            if (input_file) {
                int fd = open(input_file, O_RDONLY);
                if (fd < 0) {
                    perror("open input failed");
                    return 1;
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // Output redirection
            if (output_file) {
                int fd = open(output_file,
                              O_WRONLY | O_CREAT | O_TRUNC,
                              0644);
                if (fd < 0) {
                    perror("open output failed");
                    return 1;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

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
