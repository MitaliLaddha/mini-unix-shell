#include <stdio.h>
#include <string.h>

#define MAX_LINE 1024

int main() {
    char line[MAX_LINE];

    while (1) {
        // Print prompt
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

        // Exit condition
        if (strcmp(line, "exit") == 0) {
            break;
        }

        // echo input
        printf("You typed: %s\n", line);
    }

    return 0;
}
