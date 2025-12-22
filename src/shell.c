#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_CMDS 16

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_sigint(int sig) {
    // Ignore Ctrl+C in shell
}

void parse_args(char *cmd, char **args) {
    int i = 0;
    char *token = strtok(cmd, " ");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}

int main() {
    char line[MAX_LINE];

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

        // Split by pipes
        char *cmds[MAX_CMDS];
        int num_cmds = 0;

        char *cmd = strtok(line, "|");
        while (cmd != NULL && num_cmds < MAX_CMDS) {
            cmds[num_cmds++] = cmd;
            cmd = strtok(NULL, "|");
        }

        int prev_fd = -1;  // read end of previous pipe

        for (int i = 0; i < num_cmds; i++) {
            int pipefd[2];

            if (i < num_cmds - 1) {
                pipe(pipefd);
            }

            pid_t pid = fork();

            if (pid == 0) {
                // Child
                signal(SIGINT, SIG_DFL);

                // Read from previous pipe
                if (prev_fd != -1) {
                    dup2(prev_fd, STDIN_FILENO);
                    close(prev_fd);
                }

                // Write to next pipe
                if (i < num_cmds - 1) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                char *args[MAX_ARGS];
                parse_args(cmds[i], args);

                execvp(args[0], args);
                perror("exec failed");
                return 1;
            }

            // Parent
            if (prev_fd != -1) {
                close(prev_fd);
            }

            if (i < num_cmds - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        }

        // Wait for all children
        for (int i = 0; i < num_cmds; i++) {
            wait(NULL);
        }
    }

    return 0;
}
