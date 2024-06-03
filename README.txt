Ex1 in OS: Simple Shell Project 
Authored by Adi Aharoni
211749361

1. Description:
	This program is a custom shell implementation in C that supports alias creation, execution of shell commands, and script file processing. It features a basic command-line interface for entering commands, 	managing aliases, and running scripts.

2. Program DATABASE:
	The program maintains an alias database using a linked list structure (NodeList). Each node in the list represents an alias with its corresponding command. The program also tracks various metrics like the 	number of commands executed, alias count, script lines count, and the count of legal commands that contains apostrophes.
	Functions

	Linked List Functions:
	• free_lst: Frees the entire alias list.
	• free_node: Frees a single node in the alias list.
	• push: Adds a new alias to the list or updates an existing alias.
	• delete_node: Deletes an alias from the list by its name.
	• print_lst: Prints all aliases in the list.

	Command Execution Functions:
	• input_to_arg: Converts the input command string to an array of arguments.
	• run_script_file: Executes commands from a script file.
	• run_shell_command: Executes a shell command using fork and execvp.
	• run_input_command: Determines the type of input command (alias, unalias, source, or shell command) and processes it accordingly.

	Utility Functions:
	• free_arg: Frees the memory allocated for the arguments array.
	• skip_spaces_tabs: Skips spaces and tabs in the input string starting from the given index.
	• word_to_arg: Extracts a single argument from the input string.

3. Program Files:
	•  ex.c: Contains the source code for the shell implementation.

4. How to Compile and Run:
	To compile the program, use the following command: gcc ex1.c -o ex1 -Wall
	To run the program, use the following command: ./ex1

5. Input:
	The program takes input from the user via the command line. It supports the following commands:
	• alias name='command': Creates a new alias or updates an existing one.
	• unalias name: Removes an alias.
	• source filename.sh: Executes commands from a script file.
	• exit_shell: Exits the custom shell.
	• Other regular shell commands.

6. Output:
	The program outputs various information to the terminal:
	• A prompt displaying the number of commands executed, alias count, and script lines count.
	• List of aliases when the alias command is entered without arguments.
	• PERROR for system call errors.
	• ERR for all the other cases of errors.
	• The total count of commands with apostrophes encountered upon exiting the shell.

