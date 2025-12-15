// Demonstrates how fork() creates a separate memory space
// Parent and child do not share variable changes

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid;

    printf("Before fork\n");
    int x = 10 ; 
    pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // Child process
        x += 5 ; 
        printf("Child: x = %d\n", x);
    } else {
        // Parent process
        wait(NULL);
        printf("Parent:x = %d\n", x );
    }

    return 0;
}
