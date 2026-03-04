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
#define HISTORY_SIZE 100

pid_t stopped_pid = -1;

/* ---------- history ---------- */

char history[HISTORY_SIZE][MAX_LINE];
int history_count = 0;

void add_history(char *cmd) {
    if (strlen(cmd) == 0) return;
    strcpy(history[history_count % HISTORY_SIZE], cmd);
    history_count++;
}

void show_history(int index, char *buffer) {
    if (index < 0 || index >= history_count) return;
    strcpy(buffer, history[index % HISTORY_SIZE]);
    printf("\33[2K\rmyshell> %s", buffer);
    fflush(stdout);
}

void print_history() {
    for (int i = 0; i < history_count && i < HISTORY_SIZE; i++)
        printf("%d %s\n", i + 1, history[i]);
}

/* ---------- signals ---------- */

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_sigint(int sig) {}
void handle_sigtstp(int sig) {}

/* ---------- parsing ---------- */

void parse_args(char *cmd, char **args) {

    int i = 0;
    char *p = cmd;

    while (*p) {

        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            break;

        if (*p == '"') {

            p++;
            args[i++] = p;

            while (*p && *p != '"')
                p++;

            if (*p) {
                *p = '\0';
                p++;
            }

        } else {

            args[i++] = p;

            while (*p && *p != ' ' && *p != '\t')
                p++;

            if (*p) {
                *p = '\0';
                p++;
            }
        }

        if (i >= MAX_ARGS - 1)
            break;
    }

    args[i] = NULL;
}

/* ---------- job control ---------- */

typedef struct {
    int id;
    pid_t pid;
    char command[MAX_LINE];
    int stopped;
} Job;

Job jobs[MAX_JOBS];
int job_count = 0;

void add_job(pid_t pid, char *cmd, int stopped) {

    if (job_count >= MAX_JOBS) return;

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

/* ---------- terminal raw mode ---------- */

void enable_raw_mode(struct termios *orig) {

    struct termios raw;

    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;

    raw.c_lflag &= ~(ICANON | ECHO);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}

/* ---------- execute pipeline ---------- */

int run_pipeline(char *line, int background) {

    char *cmds[MAX_CMDS];
    int num_cmds = 0;

    char *cmd = strtok(line, "|");

    while (cmd && num_cmds < MAX_CMDS) {
        cmds[num_cmds++] = cmd;
        cmd = strtok(NULL, "|");
    }

    int prev_fd = -1;
    pid_t job_pgid = 0;
    pid_t pids[MAX_CMDS];

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

            /* ---------- redirection ---------- */

            for (int j = 0; args[j]; j++) {

                if (strcmp(args[j], "<") == 0) {

                    int fd = open(args[j+1], O_RDONLY);
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                    args[j] = NULL;
                }

                if (strcmp(args[j], ">") == 0) {

                    int fd = open(args[j+1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                    args[j] = NULL;
                }

                if (strcmp(args[j], ">>") == 0) {

                    int fd = open(args[j+1], O_CREAT | O_WRONLY | O_APPEND, 0644);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                    args[j] = NULL;
                }
            }

            execvp(args[0], args);

            perror("exec failed");
            exit(1);
        }

        if (job_pgid == 0)
            job_pgid = pid;

        setpgid(pid, job_pgid);

        if (prev_fd != -1)
            close(prev_fd);

        if (i < num_cmds - 1) {
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
    }

    if (background) {
        printf("[running in background]\n");
        return 0;
    }

    int status;

    for (int i = 0; i < num_cmds; i++)
        waitpid(pids[i], &status, WUNTRACED);

    return WEXITSTATUS(status);
}

/* ---------- main ---------- */

int main() {

    char line[MAX_LINE];
    struct termios orig_termios;

    pid_t shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    signal(SIGCHLD, handle_sigchld);
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);

    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    while (1) {

        char cwd[1024];
        getcwd(cwd, sizeof(cwd));

        printf("myshell:%s$ ", cwd);
        fflush(stdout);

        enable_raw_mode(&orig_termios);

        int pos = 0;
        int history_index = history_count;

        while (1) {

            char c;
            read(STDIN_FILENO, &c, 1);

            if (c == '\n') {
                line[pos] = '\0';
                printf("\n");
                break;
            }

            if (c == 127) {

                if (pos > 0) {
                    pos--;
                    printf("\b \b");
                    fflush(stdout);
                }
                continue;
            }

            if (c == 27) {

                char seq[2];
                read(STDIN_FILENO, &seq[0], 1);
                read(STDIN_FILENO, &seq[1], 1);

                if (seq[1] == 'A') {

                    history_index--;
                    if (history_index < 0)
                        history_index = 0;

                    show_history(history_index, line);
                    pos = strlen(line);
                }

                if (seq[1] == 'B') {

                    history_index++;
                    if (history_index >= history_count)
                        history_index = history_count - 1;

                    show_history(history_index, line);
                    pos = strlen(line);
                }

                continue;
            }

            line[pos++] = c;
            write(STDOUT_FILENO, &c, 1);
        }

        disable_raw_mode(&orig_termios);

        if (strlen(line) == 0)
            continue;

        add_history(line);

        if (strcmp(line, "exit") == 0)
            break;

        /* ---------- split by ; ---------- */

        char *commands[32];
        int cmd_count = 0;

        char *token = strtok(line, ";");

        while (token) {
            commands[cmd_count++] = token;
            token = strtok(NULL, ";");
        }

        for (int c = 0; c < cmd_count; c++) {

            char *command = commands[c];

            int background = 0;

            if (command[strlen(command) - 1] == '&') {
                background = 1;
                command[strlen(command) - 1] = '\0';
            }

            int status = run_pipeline(command, background);

            if (strstr(command, "&&") && status != 0)
                break;

            if (strstr(command, "||") && status == 0)
                break;
        }
    }

    return 0;
}