#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child
        char *args[] = {"ls", "-l", NULL};
        execvp("ls", args);

        perror("exec failed");
        return 1;
    } else {
        // Parent
        wait(NULL);
        printf("Parent: command finished\n");
    }

    return 0;
}

//Parent stays alive
// Child replaces itself with ls
//Parent waits
//Parent resumes and prints message