#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <glob.h>

#define _POSIX_C_SOURCE 200809L
#define MAX_COMMAND_LENGTH 1024
#define MAX_TOKENS 100
#define DEBUG 0

int isDynamicToken[MAX_TOKENS];

// Function prototypes
void executeBuiltIn(char *tokens[], int numTokens);
void executeWhich(const char *command);
void handleRedirectionAndExecute(char *tokens[], int numTokens);
int createAndExecutePipe(char *leftCommand[], char *rightCommand[]);
void processCommand(char *command);
void interactiveMode();
void batchMode(const char *filename);
void tokenize(char *command, char *tokens[], int *numTokens);
int findCommandPath(char *command, char *fullPath);
void executeExternalCommand(char *tokens[], int numTokens);
void expandWildcards(char *tokens[], int *numTokens);
char* custom_strdup(const char* s);

void tokenize(char *command, char *tokens[], int *numTokens) {
    char *token;
    *numTokens = 0;

    token = strtok(command, " \t\n");
    while (token != NULL && *numTokens < MAX_TOKENS - 1) {
        tokens[*numTokens] = token;
        isDynamicToken[*numTokens] = 0;  // Mark as not dynamically allocated
        (*numTokens)++;
        token = strtok(NULL, " \t\n");
    }
    tokens[*numTokens] = NULL; // Null-terminate the list of tokens
}

void processCommand(char *command) {
    static char *prevTokens[MAX_TOKENS] = {NULL}; // To track previous tokens
    static int prevNumTokens = 0; // Number of tokens in previous command

    // Free memory from the previous command
    for (int i = 0; i < prevNumTokens; ++i) {
        if (prevTokens[i] != NULL) {
            free(prevTokens[i]);
            prevTokens[i] = NULL;
        }
    }

    char *tokens[MAX_TOKENS];
    int numTokens;

    // Tokenize the input command
    tokenize(command, tokens, &numTokens);

    // Handle the case of an empty command
    if (numTokens == 0) {
        return; // Nothing to do for an empty command
    }

    // Check for the exit command
    if (strcmp(tokens[0], "exit") == 0) {
        printf("Exiting...\n");
        exit(EXIT_SUCCESS); // Exit the program
    }

    // Expand wildcards in tokens
    expandWildcards(tokens, &numTokens); 

    // First check for pipes
    char *leftCommand[MAX_TOKENS];
    char *rightCommand[MAX_TOKENS];
    int foundPipe = 0;
    int i, j = 0;

    for (i = 0; i < numTokens; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            foundPipe = 1;
            leftCommand[i] = NULL; // Null-terminate left command
            i++; // Skip pipe token
            break;
        }
        leftCommand[i] = tokens[i];
    }

    if (foundPipe) {
        for (; i < numTokens; i++) {
            rightCommand[j++] = tokens[i];
        }
        rightCommand[j] = NULL; // Null-terminate right command

        createAndExecutePipe(leftCommand, rightCommand);
        return; // Return after handling the pipe
    }

    // If no pipe is found, handle as built-in or external command
    if (strcmp(tokens[0], "cd") == 0 || strcmp(tokens[0], "pwd") == 0 || strcmp(tokens[0], "which") == 0) {
        executeBuiltIn(tokens, numTokens);
    } else {
        // If not a built-in command, handle redirection and execute
        handleRedirectionAndExecute(tokens, numTokens);
    }
    
    // Save tokens for the next command
    memcpy(prevTokens, tokens, sizeof(tokens[0]) * numTokens);
    prevNumTokens = numTokens;
}


int createAndExecutePipe(char *leftCommand[], char *rightCommand[]) {
    if(DEBUG) printf("Beginning Pipe creation\n");
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        return -1;
    }

    if (pid1 == 0) {
        // First child process
        close(pipefd[0]); // Close unused read end
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to write end of pipe
        close(pipefd[1]);

        char fullPath[MAX_COMMAND_LENGTH];
        if (!findCommandPath(leftCommand[0], fullPath)) {
            fprintf(stderr, "%s: command not found\n", leftCommand[0]);
            return -1;
        }
        if(DEBUG) printf("executing first child process, path is %s", fullPath);
        execv(fullPath, leftCommand);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        return -1;
    }

    if (pid2 == 0) {
        // Second child process
        close(pipefd[1]); // Close unused write end
        dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to read end of pipe
        close(pipefd[0]);

        char fullPath[MAX_COMMAND_LENGTH];
        if (!findCommandPath(rightCommand[0], fullPath)) {
            fprintf(stderr, "%s: command not found\n", rightCommand[0]);
            return -1;
        }
        if(DEBUG) printf("executing second child process, path is %s", fullPath);
        execv(fullPath, rightCommand);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    // Parent process
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}

void handleRedirectionAndExecute(char *tokens[], int numTokens) {
    if(DEBUG) printf("Beginnging handleRedirection, numTokens = %d\n", numTokens);
    int inRedirect = -1, outRedirect = -1; // Indices of redirection tokens if found
    char *command[MAX_TOKENS];
    int commandLength = 0;

    for (int i = 0; i < numTokens; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            inRedirect = i + 1; // Next token is the input file
            i++; // Skip the filename token
        } else if (strcmp(tokens[i], ">") == 0) {
            outRedirect = i + 1; // Next token is the output file
            i++; // Skip the filename token
        } else {
            command[commandLength++] = tokens[i];
        }
    }
    command[commandLength] = NULL; // Null-terminate the command

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        // Child process
        
        if (inRedirect != -1) {
            if(DEBUG) printf("In child process, opening %s\n", tokens[inRedirect]);
            int fdIn = open(tokens[inRedirect], O_RDONLY);
            if (fdIn == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fdIn, STDIN_FILENO);
            close(fdIn);
        }

        if (outRedirect != -1) {
            if(DEBUG) printf("In child process, opening %s\n", tokens[outRedirect]);
            int fdOut = open(tokens[outRedirect], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fdOut == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fdOut, STDOUT_FILENO);
            close(fdOut);
        }

        char fullPath[MAX_COMMAND_LENGTH];
        if (!findCommandPath(tokens[0], fullPath)) {
            fprintf(stderr, "%s: command not found\n", tokens[0]);
            return;
        }

        if(DEBUG) printf("Executing %s\n", fullPath);
        execv(fullPath, command);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
    }
}

void interactiveMode() {
    char command[MAX_COMMAND_LENGTH];

    printf("Welcome to my shell!\n");
    while (1) {
        printf("mysh> ");
        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
            break; // Exit on end of file (Ctrl+D)
        }

        processCommand(command);
    }
    printf("mysh: exiting\n");
}

void batchMode(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char command[MAX_COMMAND_LENGTH];

    while (fgets(command, MAX_COMMAND_LENGTH, file) != NULL) {
        processCommand(command);
    }

    fclose(file);
}

void executeBuiltIn(char *tokens[], int numTokens) {
    if(DEBUG) printf("Beginning executeBuiltIn, numTokens = %d\n", numTokens);
    if (strcmp(tokens[0], "cd") == 0) {
        if (numTokens != 2) {
            fprintf(stderr, "cd: wrong number of arguments\n");
        } else if (chdir(tokens[1]) != 0) {
            perror("cd");
        }
    } else if (strcmp(tokens[0], "pwd") == 0) {
        char cwd[MAX_COMMAND_LENGTH];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd");
        }
    } else if (strcmp(tokens[0], "which") == 0) {
        if (numTokens != 2) {
            fprintf(stderr, "which: wrong number of arguments\n");
        } else {
            executeWhich(tokens[1]);
        }
    }
}

void executeWhich(const char *command) {
    if(DEBUG) printf("Beginning executeWhich, command = %s\n", command);
    char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    char path[MAX_COMMAND_LENGTH];
    int found = 0;

    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s/%s", directories[i], command);
        if (access(path, X_OK) == 0) {
            printf("%s\n", path);
            found = 1;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "which: no %s in (%s, %s, %s)\n", command, directories[0], directories[1], directories[2]);
    }
}

int findCommandPath(char *command, char *fullPath) {
    if(DEBUG) printf("Beginning findCommandPath, command = %s, fullPath = %s\n", command, fullPath);
    char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    int found = 0;

    if (strchr(command, '/') != NULL) {
        // If command contains '/', it's a path to an executable
        strcpy(fullPath, command);
        return access(fullPath, X_OK) == 0;
    }

    for (int i = 0; i < 3; i++) {
        snprintf(fullPath, MAX_COMMAND_LENGTH, "%s/%s", directories[i], command);
        if (access(fullPath, X_OK) == 0) {
            found = 1;
            break;
        }
    }

    return found;
}

void executeExternalCommand(char *tokens[], int numTokens) {
    if(DEBUG) printf("Beginning executeExternalCommand, numTokens = %d\n", numTokens);
    char fullPath[MAX_COMMAND_LENGTH];
    if (!findCommandPath(tokens[0], fullPath)) {
        fprintf(stderr, "%s: command not found\n", tokens[0]);
        return;
    }

    printf("Executing command: %s\n", fullPath); // Debug print

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        // Child process
        if (execv(fullPath, tokens) == -1) {
            perror("execv");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
    }
}

void expandWildcards(char *tokens[], int *numTokens) {
    glob_t globbuf;
    int i, j;
    char *newTokens[MAX_TOKENS];
    int newNumTokens = 0;

    globbuf.gl_pathc = 0;
    globbuf.gl_pathv = NULL;
    globbuf.gl_offs = 0;

    for (i = 0; i < *numTokens; i++) {
        if (strchr(tokens[i], '*') != NULL) {
            int flags = (newNumTokens == 0 ? GLOB_NOCHECK : GLOB_APPEND | GLOB_NOCHECK);
            if (glob(tokens[i], flags, NULL, &globbuf) != 0) {
                continue;
            }

            if (globbuf.gl_pathc == 0) {
                newTokens[newNumTokens++] = custom_strdup(tokens[i]);
            } else {
                for (j = 0; j < globbuf.gl_pathc; j++) {
                    newTokens[newNumTokens++] = custom_strdup(globbuf.gl_pathv[j]);
                }
            }
        } else {
            newTokens[newNumTokens++] = custom_strdup(tokens[i]);
        }
    }

    // Replace original tokens with expanded tokens
    for (i = 0; i < newNumTokens; i++) {
        tokens[i] = newTokens[i];
    }
    *numTokens = newNumTokens;

    globfree(&globbuf);
}




char* custom_strdup(const char* s) {
    char* new_str = malloc(strlen(s) + 1); // +1 for the null terminator
    if (new_str) {
        strcpy(new_str, s);
    }
    return new_str;
}


int main(int argc, char *argv[]) {
    if (argc == 2) {
        batchMode(argv[1]);
    } else if (argc == 1) {
        interactiveMode();
    } else {
        fprintf(stderr, "Usage: %s [scriptfile]\n", argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
