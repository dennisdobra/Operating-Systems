# Minishell

## Objectives
- Learn how shells create new child processes and connect I/O to the terminal.
- Gain a better understanding of the `fork()` function wrapper.
- Learn to correctly execute commands written by the user and handle errors.

## Introduction
A shell is a command-line interpreter that provides a text-based user interface for operating systems. Bash is both an interactive command language and a scripting language, allowing users to interact with the file system, applications, and the operating system.

This project involves building a Bash-like shell with minimal functionalities, including traversing the file system, running applications, redirecting their output, and piping the output from one application to another.

## Shell Functionalities

### Changing the Current Directory
The shell supports a built-in command for navigating the file system, `cd`. You will need to store the current directory path, as users can provide either relative or absolute paths as arguments.

The built-in `pwd` command will show the current directory path. Here are some examples:

> pwd
/home/student
> cd operating-systems/assignments/minishell
> pwd
/home/student/operating-systems/assignments/minishell
> cd inexitent
no such file or directory
> cd /usr/lib
> pwd
/usr/lib


**Note:** Using the `cd` command without any arguments or with more than one argument will not affect the current directory path.

### Closing the Shell
Inputting either `quit` or `exit` should close the minishell.

### Running an Application
Suppose you have an executable named `sum` in the current directory. It takes multiple numbers as arguments and prints their sum to `stdout`. Here’s how it should behave:

./sum 2 4 1 7


If the executable is located at an absolute path, it should also work:

/home/student/sum 2 4 1 7

Each application will run in a separate child process created using `fork`.

### Environment Variables
Your shell will support the use of environment variables, inherited from the bash process that started your minishell application. If an undefined variable is used, its value will be an empty string (`""`).

Example:
NAME="John Doe"
AGE=27
./identify $NAME $LOCATION $AGE

translates to ./identify "John Doe" "" 27

### Operators
#### Sequential Operator
The `;` operator chains multiple commands to run sequentially:
echo "Hello"; echo "world!"; echo "Bye!" Hello world! Bye!

#### Parallel Operator
The `&` operator runs commands in parallel:
echo "Hello" & echo "world!" & echo "Bye!"

The words may be printed in any order

#### Pipe Operator
The `|` operator redirects output from one command to the input of another:
echo "world" | ./reverse_input # output of echo used as input for reverse_input

#### Chain Operators for Conditional Execution
- `&&` executes commands sequentially until one fails:
echo "H" && echo "e" && echo "l" && ./throw_error && echo "l" && echo "o" H e l ERROR: I always fail

- `||` executes commands until one succeeds:
./throw_error || echo "Hello" || echo "world!" ERROR: I always fail Hello

### Operator Priority
The priority of operators is as follows (lower number = higher priority):
1. Pipe operator (`|`)
2. Conditional execution operators (`&&`, `||`)
3. Parallel operator (`&`)
4. Sequential operator (`;`)

### I/O Redirection
The shell supports the following redirection options:
- `< filename` - redirects filename to standard input
- `> filename` - redirects standard output to filename
- `2> filename` - redirects standard error to filename
- `&> filename` - redirects both standard output and error to filename
- `>> filename` - appends standard output to filename
- `2>> filename` - appends standard error to filename

## Testing
Automated tests are located in the `inputs/` directory. To execute tests, run:
```bash
./run_all.sh
Debugging
To compare the output of the minishell with a reference binary, set DO_CLEANUP=no in _test/run_test.sh. Check results in the _test/outputs/ directory.

Memory Leaks
To inspect unreleased resources, set USE_VALGRIND=yes and DO_CLEANUP=no in _test/run_test.sh. Modify the path to the Valgrind log file as needed.

Checkpatch
Use the checkpatch.pl script from the official Linux kernel repository to check for coding style issues. Run:

./checkpatch.pl --no-tree --terse -f /path/to/your/code.