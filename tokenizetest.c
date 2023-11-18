#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEBUG 1

#define TOKENSIZE 8

typedef struct tokens
{
    char *tok;
    struct tokens *next;
} tokens;

// duplicates string
char *my_strdup(const char *s) {
    if (s == NULL) {
        return NULL;
    }
    char *d = malloc(strlen(s) + 1);
    if (d == NULL) {
        return NULL;
    }
    strcpy(d, s);
    return d;
}


// Returns 1 if passed argument is a valid character in a command 
int is_valid(char c) {
    return (strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz*.<>|/", c) != NULL);
}

// tokenize gets passed the entire command entered
// Split command into individual tokens, fills them into a token struct 
tokens* tokenize(char *command){

    int len = strlen(command);

    tokens *head = malloc(sizeof(tokens));
    tokens *curTok = head;
    
    char *current = malloc(TOKENSIZE);
    int count = 0;
    int nextNew;

    if(DEBUG) printf("%d", len);
    // for every character in command
    // returns the head of the linked list of tokens
    for(int i = 0; i < len; i++){
	
	if(command[i] == '\n'){
        current[count + 1] = '\n';
        curTok->tok = my_strdup(current);
        free(current);

        tokens* temp = head;
        while (temp != NULL) {
            tokens* next = temp->next;
            free(temp->tok);
            free(temp);
            temp = next;
        }
        return head;
    }

        if(command[i] == ' ' && i != 0){
            current[count + 1] = '\n';
            curTok->tok = current;
            nextNew = 1; 
            count = 0;
        } 

        if(is_valid(command[i])){

            if(count >= TOKENSIZE){
                current = realloc(current, sizeof(char) * (count * 2));
            }

            if(nextNew){
                curTok->next = malloc(sizeof(tokens));
                curTok->next->next = NULL;

                nextNew = 0;
            }

            current[count] = command[i];
            count++;
        }

    }

    
    tokens* temp = head;
    while (temp != NULL) {
        tokens* next = temp->next;
        free(temp->tok);
        free(temp);            
        temp = next;
    }
    
    return head;

}


int main(int argc, char **argv){

    char com[128];
    char *command = com;

    printf("mysh> ");
    fgets(command, 128, stdin);

    tokens *current = malloc(sizeof(tokens));
    current = tokenize(command);
    
    while(current != NULL){
        printf("%s\n", current->tok);
        current = current->next;
    }

    return EXIT_SUCCESS;
}
