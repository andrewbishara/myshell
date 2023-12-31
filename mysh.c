#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <glob.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_TOKENS 100
#define DEBUG 0

int isDynamicToken[MAX_TOKENS];
int lastExitStatus;

// Function prototypes
void executeBuiltIn(char *tokens[], int numTokens);
void executeWhich(const char *command);
int handleRedirectionAndExecute(char *command[], const char *outputFile);
int createAndExecutePipe(char *leftCommand[], char *rightCommand[]);
void processCommand(char *command);
void interactiveMode();
void batchMode(const char *filename);
void tokenize(char *command, char *tokens[], int *numTokens);
int findCommandPath(char *command, char *fullPath);
void executeExternalCommand(char *tokens[], int numTokens);
void expandWildcards(char *tokens[], int *numTokens);
char* custom_strdup(const char* s);
void freeDynamicTokens(char *tokens[]);
void shiftTokens(char *tokens[]);
int createAndExecutePipeWithRedirection(char *leftCommand[], char *rightCommand[], const char *outputFile);

//Takes inputed command, seperates it into tokens, inserts them into given array 
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

//Driver function, handles all cases with use of helper functions
void processCommand(char *command) {
    static char *prevTokens[MAX_TOKENS];
    static int prevIsDynamicToken[MAX_TOKENS];

    // Free memory from the previous command
    for (int i = 0; i < MAX_TOKENS; ++i) {
        if (prevIsDynamicToken[i] && prevTokens[i] != NULL) {
            free(prevTokens[i]);
            prevTokens[i] = NULL;
        }
    }

    char *tokens[MAX_TOKENS];
    int numTokens;
    memset(isDynamicToken, 0, sizeof(isDynamicToken)); // Reset dynamic token flags

    // Tokenize the input command
    tokenize(command, tokens, &numTokens);

    if (numTokens == 0) return; // Handle empty command

    // Check for the exit command
    if (strcmp(tokens[0], "exit") == 0) {
        printf("Exiting...\n");
        exit(EXIT_SUCCESS); // Exit the program
    }

    // Conditional execution handling
    if (strcmp(tokens[0], "then") == 0 && lastExitStatus != 0) {
        return; // Skip execution if last command failed
    }
    if (strcmp(tokens[0], "else") == 0 && lastExitStatus == 0) {
        return; // Skip execution if last command succeeded
    }

    // Expand wildcards in tokens
    expandWildcards(tokens, &numTokens);

    // Handle pipes and redirections
    char *leftCommand[MAX_TOKENS];
    char *rightCommand[MAX_TOKENS];
    char *outputFile = NULL;
    int pipeFound = 0, redirectFound = 0;
    int leftNumTokens = 0, rightNumTokens = 0;

    for (int i = 0; i < numTokens; ++i) {
        if (strcmp(tokens[i], "|") == 0) {
            pipeFound = 1;
            leftCommand[leftNumTokens] = NULL;
            continue;
        }

        if (strcmp(tokens[i], ">") == 0) {
            redirectFound = 1;
            outputFile = tokens[++i]; // Assume next token is the file name
            continue;
        }

        if (!pipeFound) {
            leftCommand[leftNumTokens++] = tokens[i];
        } else {
            rightCommand[rightNumTokens++] = tokens[i];
        }
    }

    if (pipeFound) {
        rightCommand[rightNumTokens] = NULL;
        // Handle piping with redirection if needed
        if (redirectFound) {
            lastExitStatus = createAndExecutePipeWithRedirection(leftCommand, rightCommand, outputFile);
        } else {
            lastExitStatus = createAndExecutePipe(leftCommand, rightCommand);
        }
    } else {
        leftCommand[leftNumTokens] = NULL;
        // Handle single command with or without redirection
        if (redirectFound) {
            lastExitStatus = handleRedirectionAndExecute(leftCommand, outputFile);
        } else {
            lastExitStatus = handleRedirectionAndExecute(leftCommand, NULL);
        }
    }

    // Clear tokens and flags for next iteration
    memset(prevTokens, 0, sizeof(prevTokens));
    memset(prevIsDynamicToken, 0, sizeof(prevIsDynamicToken));
}

//Helper function to execute any commands including pipes 
// Returns exit status of ran program 
int createAndExecutePipe(char *leftCommand[], char *rightCommand[]) {
    if(DEBUG) printf("Beginning Pipe creation\n");
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }

    int exitStatus = 0;
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
            exit(EXIT_FAILURE);
        }
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
            exit(EXIT_FAILURE);
        }
        execv(fullPath, rightCommand);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    // Parent process
    close(pipefd[0]);
    close(pipefd[1]);

    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    if (WIFEXITED(status1) && WEXITSTATUS(status1) != 0) {
        exitStatus = WEXITSTATUS(status1);
    }
    if (WIFEXITED(status2) && WEXITSTATUS(status2) != 0) {
        exitStatus = WEXITSTATUS(status2);
    }

    return exitStatus;
}

//Helper function to execute commands that have both pipes and redirections
//Returns exit status of the executed program
int createAndExecutePipeWithRedirection(char *leftCommand[], char *rightCommand[], const char *outputFile) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid1 == 0) { // Child process for leftCommand
        // Redirect STDOUT to the write end of the pipe
        close(pipefd[0]); // Close unused read end
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        char fullPath[MAX_COMMAND_LENGTH];
        if (!findCommandPath(leftCommand[0], fullPath)) {
            fprintf(stderr, "%s: command not found\n", leftCommand[0]);
            exit(EXIT_FAILURE);
        }
        execv(fullPath, leftCommand);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid2 == 0) { // Child process for rightCommand
        // Redirect STDIN to the read end of the pipe
        close(pipefd[1]); // Close unused write end
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        // Redirect STDOUT to the output file
        int fdOut = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fdOut == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fdOut, STDOUT_FILENO);
        close(fdOut);

        char fullPath[MAX_COMMAND_LENGTH];
        if (!findCommandPath(rightCommand[0], fullPath)) {
            fprintf(stderr, "%s: command not found\n", rightCommand[0]);
            exit(EXIT_FAILURE);
        }
        execv(fullPath, rightCommand);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    // Parent process
    close(pipefd[0]);
    close(pipefd[1]);

    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    // Return the exit status of the last command in the pipe
    if (WIFEXITED(status2)) {
        return WEXITSTATUS(status2);
    } else {
        return -1;
    }
}

//Helper function to execute any command including redirections
// Returns exit status of the executed program
int handleRedirectionAndExecute(char *command[], const char *outputFile) {
    if(DEBUG) printf("Beginning handleRedirection\n");

    int inRedirect = -1; // Index of input redirection if found
    int outRedirect = outputFile ? 1 : -1; // Set to 1 if output redirection is present
    int commandLength = 0;

    // Identifying input redirection
    while (command[commandLength] != NULL) {
        if (strcmp(command[commandLength], "<") == 0) {
            inRedirect = commandLength + 1;
            command[commandLength] = NULL; // Terminate the command array here
            break; // Stop processing further as we hit redirection
        }
        commandLength++;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) { // Child process
        if (inRedirect != -1) {
            int fdIn = open(command[inRedirect], O_RDONLY);
            if (fdIn == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fdIn, STDIN_FILENO);
            close(fdIn);
        }

        if (outRedirect != -1) {
            int fdOut = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fdOut == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fdOut, STDOUT_FILENO);
            close(fdOut);
        }

        char fullPath[MAX_COMMAND_LENGTH];
        if (!findCommandPath(command[0], fullPath)) {
            fprintf(stderr, "%s: command not found\n", command[0]);
            exit(EXIT_FAILURE);
        }

        if(DEBUG) printf("Executing %s\n", fullPath);
        execv(fullPath, command);
        perror("execv");
        exit(EXIT_FAILURE);
    } else { // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return -1; // Or some other error code
        }
    }
}

// Main loop for running interactive mode
// Uses processCommand() as driving function
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

// Main loop for running batch mode
// Uses processCommand() as driving function
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

//runs cd, pwd, and which commands (Built ins)
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

// Helper function that executes the which command
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

//takes a program name, constructs the path to it and sets fullPath equal to the path
//Returns 1 if a valid path was built, 0 if not
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

//Executes any command not built in
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
            lastExitStatus = EXIT_FAILURE;
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
    }

    lastExitStatus = EXIT_FAILURE;
    exit(EXIT_SUCCESS);
}

//Searches if any tokens contain a wildcard, expands them, inserts them into main token array
void expandWildcards(char *tokens[], int *numTokens) {
    glob_t globbuf;
    int i, j;
    char *newTokens[MAX_TOKENS];
    int newNumTokens = 0;

    globbuf.gl_pathc = 0;
    globbuf.gl_pathv = NULL;
    globbuf.gl_offs = 0;

    for (i = 0; i < *numTokens; i++) {
        // Check if token contains a wildcard
        if (strchr(tokens[i], '*') != NULL) {
            int flags = (newNumTokens == 0 ? GLOB_NOCHECK : GLOB_APPEND | GLOB_NOCHECK);
            if (glob(tokens[i], flags, NULL, &globbuf) != 0) {
                continue;
            }

            for (j = 0; j < globbuf.gl_pathc; j++) {
                newTokens[newNumTokens] = custom_strdup(globbuf.gl_pathv[j]);
                isDynamicToken[newNumTokens] = 1;
                newNumTokens++;
            }
        } else {
            newTokens[newNumTokens] = tokens[i];
            isDynamicToken[newNumTokens] = 0;
            newNumTokens++;
        }
    }

    *numTokens = newNumTokens;
    memcpy(tokens, newTokens, sizeof(char*) * MAX_TOKENS);
    
    globfree(&globbuf);
}


// Helper function to free dynamically allocated tokens
void freeDynamicTokens(char *tokens[]) {
    for (int i = 0; tokens[i] != NULL; i++) {
        if (isDynamicToken[i]) {
            free(tokens[i]);
            tokens[i] = NULL;
        }
    }
}

// Helper function to remove first index of tokens array 
void shiftTokens(char *tokens[]){        
    char *newTokens[MAX_TOKENS];
    for(int i = 0; i < MAX_TOKENS - 1; i++){

        if(DEBUG) printf("Previous token %s going into index %d", tokens[i+1], i);
        newTokens[i] = tokens[i+1];
    }

    for(int j = 0; j < MAX_TOKENS; j++){
        tokens[j] = newTokens[j];
    }

}

//Duplicates given string
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
