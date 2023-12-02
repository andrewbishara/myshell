#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]){
    int square;
    for(int i = 0; i < 10; i++){
        square = i * i;
        printf("%d", square);
    }

    return(EXIT_SUCCESS);
}