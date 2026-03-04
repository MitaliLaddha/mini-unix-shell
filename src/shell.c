#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_CMDS 16
#define MAX_JOBS 64

pid_t stopped_pid = -1;

/* reap background children */
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ignore ctrl+c in shell */
void handle_sigint(int sig) {
}

/* ignore ctrl+z in shell */
void handle_sigtstp(int sig) {
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

typedef struct {
    int id;
    pid_t pid;
    char command[MAX_LINE];
    int stopped;
} Job;

Job jobs[MAX_JOBS];
int job_count = 0;

void add_job(pid_t pid, char *cmd, int stopped) {

    if (job_count >= MAX_JOBS)
        return;

    jobs[job_count].id = job_count + 1;
    jobs[job_count].pid = pid;
    strcpy(jobs[job_count].command, cmd);
    jobs[job_count].stopped = stopped;

    job_count++;
}

void print_jobs() {

    for (int i = 0; i < job_count; i++) {

        printf("[%d] PID:%d ", jobs[i].id, jobs[i].pid);

        if (jobs[i].stopped)
            printf("Stopped ");
        else
            printf("Running ");

        printf("%s\n", jobs[i].command);
    }
}

void resume_background() {

    if (stopped_pid <= 0) {
        printf("No stopped job\n");
        return;
    }

    kill(-stopped_pid, SIGCONT);

    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == stopped_pid) {
            jobs[i].stopped = 0;
            printf("[%d] Running %s\n", jobs[i].id, jobs[i].command);
            return;
        }
    }
}

int main() {

    char line[MAX_LINE];

    /* shell job-control setup */
    pid_t shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    signal(SIGCHLD, handle_sigchld);
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);

    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    while (1) {

        printf("myshell> ");
        fflush(stdout);

        if (fgets(line, MAX_LINE, stdin) == NULL) {
            printf("\n");
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "exit") == 0)
            break;

        /* split pipeline */
        char *cmds[MAX_CMDS];
        int num_cmds = 0;

        char *cmd = strtok(line, "|");

        while (cmd != NULL && num_cmds < MAX_CMDS) {
            cmds[num_cmds++] = cmd;
            cmd = strtok(NULL, "|");
        }

        /* built-in commands */
        if (num_cmds == 1) {

            char temp[MAX_LINE];
            strcpy(temp, cmds[0]);

            char *args[MAX_ARGS];
            parse_args(temp, args);

            if (args[0] && strcmp(args[0], "fg") == 0) {

                if (stopped_pid > 0) {

                    tcsetpgrp(STDIN_FILENO, stopped_pid);
                    kill(-stopped_pid, SIGCONT);

                    waitpid(-stopped_pid, NULL, WUNTRACED);

                    tcsetpgrp(STDIN_FILENO, shell_pgid);

                    stopped_pid = -1;

                } else {
                    printf("No stopped job\n");
                }

                continue;
            }

            if (args[0] && strcmp(args[0], "bg") == 0) {
                resume_background();
                continue;
            }

            if (args[0] && strcmp(args[0], "jobs") == 0) {
                print_jobs();
                continue;
            }
        }

        int prev_fd = -1;
        pid_t pids[MAX_CMDS];
        pid_t job_pgid = 0;

        for (int i = 0; i < num_cmds; i++) {

            int pipefd[2];

            if (i < num_cmds - 1)
                pipe(pipefd);

            pid_t pid = fork();
            pids[i] = pid;

            if (pid == 0) {

                if (job_pgid == 0)
                    job_pgid = getpid();

                setpgid(0, job_pgid);

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
                int append = 0;

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

                    if (strcmp(args[j], ">>") == 0) {

                        output_file = args[j + 1];
                        append = 1;
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

                    int fd;

                    if (append)
                        fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    else
                        fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

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

            if (job_pgid == 0)
                job_pgid = pid;

            setpgid(pid, job_pgid);

            if (i == 0)
                tcsetpgrp(STDIN_FILENO, job_pgid);

            if (prev_fd != -1)
                close(prev_fd);

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
                add_job(pids[i], cmds[0], 1);

                tcsetpgrp(STDIN_FILENO, shell_pgid);

                printf("\n[process %d stopped]\n", pids[i]);
            }
        }

        tcsetpgrp(STDIN_FILENO, shell_pgid);
    }

    return 0;
}