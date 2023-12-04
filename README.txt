Project 1: My Shell

Andrew Bishara- ab2761
Blake Rears- bar181



DESIGN NOTES:

Primary Loop:
  -> The entire program runs using the same functions whether in batch or interactive mode. The
  main functions sole use is to determine which to use, then call the corresponding function.
  These functions, named batchMode() and interactiveMode(), both have one loop that runs until
  either there is no more input in the given .txt file (batch mode), or until the user ends the 
  program in interactive mode. Both of these loops work by passing each command into the
  processCommand() function, described below. 

ProcessCommand() Function:
  -> The function interprets and executes user commands. It begins by tokenizing the input 
  command into individual words or tokens, which are then analyzed to determine the nature of the 
  command. This function handles various scenarios, including empty commands, exit commands, and 
  conditional execution based on the outcome of the previous command. It also manages wildcard 
  expansion, dynamically adapting the command tokens to match filenames and other patterns in the 
  file system. The function intelligently identifies and separates piping and redirection elements 
  within the command. For commands involving pipes, it delegates the execution to 
  createAndExecutePipe or createAndExecutePipeWithRedirection, depending on whether output 
  redirection is also involved. For commands without pipes, it handles potential redirections and 
  executes either built-in commands (like `cd`, `pwd`, `which`) or external commands. The function 
  also carefully manages memory, particularly for dynamically allocated tokens, ensuring robust and 
  leak-free execution. In essence, processCommand acts as the command interpreter and executor, 
  translating user input into actions and delegating complex tasks to specialized sub-functions 
  within the shell program.

ExpandWildcards() Function:
  -> This function plays a crucial role in handling wildcard characters, such as `*`, in command 
  inputs. It scans the command tokens and, upon encountering a token containing a wildcard, utilizes 
  the `glob` function to match this pattern with existing file paths. For each match found, the 
  function dynamically allocates memory to store these file paths, effectively replacing the wildcard 
  token with a list of actual filenames or directories. Tokens without wildcards remain unaffected. 
  This mechanism allows your shell to interpret and expand wildcard patterns, enabling users to 
  execute commands on multiple files or directories simultaneously.

CreateAndExecutePipe() Function:
  -> This function allows the output of one command to be used as the input for another. This 
  function sets up a pipeline between two commands, defined by the `leftCommand` and `rightCommand` 
  arrays. It creates a pipe using the `pipe()` system call, then forks two child processes. In the 
  first child process, it redirects the standard output to the write end of the pipe, and then 
  executes the leftCommand using execv(). In the second child process, it redirects the standard 
  input to the read end of the pipe and executes the rightCommand. The parent process waits for 
  both children to finish, ensuring proper command execution and data transfer between the two 
  commands through the pipe. This function elegantly captures the essence of Unix pipelines, 
  enabling complex command sequences.

HandleAndExecuteRedirection() Function:
  -> This function allows the users to use a file as the input or output of a given command/program.
  The logic in this program works similarly as the createAndExecutePipe() function described above,
  with a few changes. This function opens the given file, and either sets it as the standard input
  or standard output of the executable on the other side of the given command, depending on 
  the direction of the redirection. It then finds, builds the path, and executes the command. Like 
  in the createAndExecutePipe(), the parent process waits for the child to finish to ensure proper 
  command execution. 

CreateAndExecutePipeWithRedirection() Function:
  -> This function combines the capabilities of piping and output redirection. This function is
    designed to execute two commands, specified by leftCommand and rightCommand, with the output
    of the rightCommand being redirected to a file specified by outputFile. The process involves
    creating a pipe and forking two child processes. In the first child process, it handles the
    leftCommand, redirecting its output to the write end of the pipe. The second child process
    deals with the rightCommand, taking its input from the read end of the pipe and
    redirecting its output to the specified file. This is achieved using file descriptors
    and the `dup2()` system call. After setting up these redirections, each child process
    executes its respective command using execv(). The parent process waits for both child 
    processes to complete. This function allows complex command execution where the output of 
    one command can be processed by another and then saved directly to a file.

FindCommandPath():
  -> This is a crucial function to help execute any commands that don't aleady include a command 
  path. This just iterates through the required paths and checks to see if the bare name exists,
  then sets a given variable to that path (if it exists). This is a very simple but incredibly 
  useful function, that a lot of other functions call.

ExecuteBuiltIns() Function:
  -> This is a relatively simple function that deals with any of the functions required to be built
  in. For dealing with the 'cd' command, it checks to make sure it is getting called properly
  then uses chdir(). To deal with 'pwd', we use the getpwd() function and print the returned path.
  Dealing with 'which' uses a helper function to work. This function builds and prints the path to 
  a given program. 



TESTING PLAN:

  -> Here is a list of deliverables that we needed to test to ensure our program meets the project
  requirements:

        -> Be able to work with either 1 or no arguments
        -> Program terminates when exit command is typed 
        -> Wildcard:
          -> add all names to the argument list that match given pattern
          -> add original token to argument list if no matches are found
        -> Redirection:
          -> changes stdin to a program to given file
          -> changes stdout from a program to given file
          -> creates new file if given file does not exist in the folder
        -> Pipes:
          -> runs first sequential program first and sends output into second program
          -> if pipe can't be created, print error messages
        -> Conditionals:
          -> program keeps track of the exit status of last executed command 
          -> runs 'then' command if previous execution is successful, not otherwise
          -> runs 'else' command if previous execution is unsuccessful, not otherwise
        -> Pathnames:
          -> paths that are given are used as is
          -> program names with no path are searched for, then ran if they exist
        -> Built Ins:
          -> the 3 required built in programs must run as they would on a regular shell
        -> Combined pipes/redirections:
          -> program must be able to handle a command with any combination of these three

TESTING STRATEGY:

-> All of these tests were done in both interactive and batch mode. The file used to run these tests
are included in the tar file

-> Exiting:
  -> This was tested by running the 'exit' command

-> Wildcards:
  -> to test basic function, we used the ls command. This was a very easy way to tell exactly whether or not
  the wildcards expanded properly, as the output is very straightforward
  -> we had to test all three different instances of a wildcard, either beginning and argument, ending an
  argument, or in the middle of one 
    -> this was tested by using ls with different arguments (ex: ls *.c, mysh.*, m*h)
  -> used ls with wildcards that didn't correspond with any files to make sure that was working properly
-> Redirection:
  -> running "ls > testoutput.txt" or "testinput.txt < sort" tested how basic calls using redirections work
  with both input and output redirection
  -> by doing "ls > testoutput.txt" both before and after making the textfile tests whether or not the program
  can handle using an existing file or making a new one
-> Pipes:
  -> We made 2 files that are included, testwrite and testread. The first one sends output to stdout, the second
  reads from stdin. 
    -> by using the command ./testwrite | ./testread, you can test wheter or not the pipes work properly by
    looking at the output of testread, and making sure it matches what testwrite writes
  -> Using a command with non-existent files tests whether or not pipes handle when a pipe can't get created 
  properly
-> Conditionals:
  -> By execute a succesful command, and then entering a command with then or else, you can test how both 
  situations are handled after succesful command. then should run, else should not
  -> The other situation also needs to be tested. Enter a command you know will fail, such as "which" with no
  arguments, then enter both then or else. Else should run, then should not
-> Pathnames:
  -> These are tested indirectly by running programs. If a path name is not properly built, the program will not
  run. Throughout all other testing, we were looking out for any errors that showed up while trying to run a program
  we knew should run
-> Built-ins:
  -> These were tested by entering commands that included these in them. This was fairly straightforward. 
-> Combined commands:
  -> we used commands that had different combinations of redirects and pipes such as:
    -> ls *.c | sort > testfile.txt
    -> ls -l | grep .txt > testfile.txt
    -> cat < input.txt | grep keyword > filtered_output.txt



