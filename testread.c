#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    char buffer[10];
    read(STDIN_FILENO, buffer, 10);
    int root;

    for (int i = 0; i < 10; i++) {
        if (buffer[i] != 0) {
            root = buffer[i] / buffer[i];
            printf("The root of %d is %d\n", i, root);
        } else {
            printf("Cannot compute the root for %d (division by zero)\n", i);
        }
    }

    return EXIT_SUCCESS;
}
