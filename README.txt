1. Tokenization and Command Processing: The program starts by breaking down user input into tokens and then processing these commands. This involves handling different scenarios like empty commands, exit commands, and conditional execution based on the previous command's exit status.

2. Piping and Redirection: A feature of the shell is its ability to handle pipes (`|`) and redirections (`<`, `>`). This includes the ability to handle complex commands where output from one command is piped into another and potentially redirected into a file.

3. Wildcard Expansion: Our shell supports wildcard expansion using `glob`, which allows commands like `ls *.txt` to work as expected.

4. Executing Built-in and External Commands: The shell distinguishes between built-in commands (like `cd`, `pwd`, `which`) and external commands, executing them appropriately.

5. Dynamic Memory Management: The program carefully manages memory, especially when dealing with dynamically allocated tokens resulting from wildcard expansion, ensuring there are no memory leaks.

6. Custom Functions: The `createAndExecutePipeWithRedirection` function is a testament to the shell's advanced capabilities, managing piping combined with output redirection efficiently.

7. Interactive and Batch Modes: Our shell operates in both interactive and batch modes, offering flexibility in how commands are entered and executed.
