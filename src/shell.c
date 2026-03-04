#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_CMDS 16

pid_t stopped_pid = -1;

/* reap background children */
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ignore ctrl+c in shell */
void handle_sigint(int sig) {
}

/* split command into argv */
void parse_args(char *cmd, char **args) {

    int i = 0;

    char *token = strtok(cmd, " \t");

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t");
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

        /* split pipeline */
        char *cmds[MAX_CMDS];
        int num_cmds = 0;

        char *cmd = strtok(line, "|");

        while (cmd != NULL && num_cmds < MAX_CMDS) {
            cmds[num_cmds++] = cmd;
            cmd = strtok(NULL, "|");
        }

        /* fg command (only if single command) */
        if (num_cmds == 1) {

            char temp[MAX_LINE];
            strcpy(temp, cmds[0]);

            char *args[MAX_ARGS];
            parse_args(temp, args);

            if (args[0] && strcmp(args[0], "fg") == 0) {

                if (stopped_pid > 0) {

                    kill(stopped_pid, SIGCONT);
                    waitpid(stopped_pid, NULL, 0);
                    stopped_pid = -1;

                } else {
                    printf("No stopped job\n");
                }

                continue;
            }
        }

        int prev_fd = -1;
        pid_t pids[MAX_CMDS];

        for (int i = 0; i < num_cmds; i++) {

            int pipefd[2];

            if (i < num_cmds - 1) {
                pipe(pipefd);
            }

            pid_t pid = fork();
            pids[i] = pid;

            if (pid == 0) {

                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);

                if (prev_fd != -1) {
                    dup2(prev_fd, STDIN_FILENO);
                    close(prev_fd);
                }

                if (i < num_cmds - 1) {

                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                char *args[MAX_ARGS];
                parse_args(cmds[i], args);

                char *input_file = NULL;
                char *output_file = NULL;

                for (int j = 0; args[j] != NULL; j++) {

                    if (strcmp(args[j], "<") == 0) {

                        input_file = args[j + 1];
                        args[j] = NULL;
                        break;
                    }

                    if (strcmp(args[j], ">") == 0) {

                        output_file = args[j + 1];
                        args[j] = NULL;
                        break;
                    }
                }

                if (input_file) {

                    int fd = open(input_file, O_RDONLY);

                    if (fd < 0) {
                        perror("open input failed");
                        exit(1);
                    }

                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                if (output_file) {

                    int fd = open(output_file,
                                  O_WRONLY | O_CREAT | O_TRUNC,
                                  0644);

                    if (fd < 0) {
                        perror("open output failed");
                        exit(1);
                    }

                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                execvp(args[0], args);

                perror("exec failed");
                exit(1);
            }

            if (prev_fd != -1) {
                close(prev_fd);
            }

            if (i < num_cmds - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        }

        /* wait for children */
        for (int i = 0; i < num_cmds; i++) {

            int status;

            waitpid(pids[i], &status, WUNTRACED);

            if (WIFSTOPPED(status)) {

                stopped_pid = pids[i];

                printf("\n[process %d stopped]\n", pids[i]);
            }
        }
    }

    return 0;
}