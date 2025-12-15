#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Before exec\n");

    char *args[] = {"ls", "-l", NULL};

    execvp("ls", args);

    // this line runs only if exec fails
    perror("exec failed");

    return 1;
}
