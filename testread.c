#include <stdio.h>

#define BUFFER_SIZE 256

int main() {
    char buffer[BUFFER_SIZE];

    printf("Program 2 is waiting for input from stdin...\n");

    // Read from stdin until EOF
    while (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
        printf("Program 2 received: %s", buffer);
    }

    printf("End of Program 2.\n");

    return 0;
}
