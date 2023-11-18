#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TOKENSIZE 8

typedef struct tokens
{
    char *tok;
    struct tokens *next;
} tokens;


// Returns 1 if passed argument is a valid character in a command 
int is_valid(char c) {
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '*' || c == '.'|| c == '>' || c == '<' || c == '|' || c == '/');
}

// tokenize gets passed the entire command entered
// Split command into individual tokens, fills them into a token struct 
tokens* tokenize(char *command){

    int len = strlen(command);
    char line[len];
    line[0] = *command;

    tokens *head = malloc(sizeof(tokens));
    tokens *curTok = malloc(sizeof(tokens));
    
    char *current = malloc(TOKENSIZE);
    int count = 0;
    int nextNew;

    // for every character in command
    // returns the head of the linked list of tokens
    for(int i = 0; i < len; i++){

        if(line[i] == '\n' && i != 0)
            return head;

        if(line[i] == ' ' && i != 0){
            if(head->tok == NULL){
                head->tok = current;
            }
            else{
                curTok->tok = current;
            }
            
            nextNew = 1; 
            current = NULL;
        } 

        if(is_valid(line[i])){

            if(count >= TOKENSIZE){
                current = realloc(current, sizeof(current) * 2);
            }

            if(nextNew){
                curTok->next = malloc(sizeof(tokens));
                curTok = curTok->next;

                nextNew = 0;
            }

            current[count] = line[i];
            count++;
        }

    }

}



int main(int argc, char **argv){

    char *command;

    printf("mysh> ");
    fgets(command, 128, stdin);

    tokens *current = tokenize(command);
    
    while(current->next != NULL){
        printf("%p\n", (void*) current->tok);
    }

    return EXIT_SUCCESS;
}
