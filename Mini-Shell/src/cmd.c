// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal print working directory command.
 */
static bool shell_pwd(word_t *dir, int fd)
{
	if (dir != NULL && dir->string != NULL) {
		fprintf(stderr, "Error: pwd does not take any arguments.\n");
		return -1;
	}

	// Get the current working directory
	char cwd[1024];

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("getcwd");
		return -1;
	}

	// Print the current working directory
	dprintf(fd, "%s\n", cwd);

	return 0;
}

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* Execute cd. */
	if (dir == NULL || dir->string == NULL) {
		fprintf(stderr, "Error: Invalid directory argument.\n");
		return -1;
	}

	// Get the directory path as a string
	char *new_path = get_word(dir);

	// Check if the path is relative
	if (new_path[0] == '/') {
		chdir(new_path);
		return 0;
	}

	// Make the path absolute by combining it with the current working directory
	char cwd[1024];

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("getcwd");
		return -1;
	}

	// Combine the current working directory and the relative path
	strcat(cwd, "/");
	strcat(cwd, new_path);

	if (chdir(cwd) == -1) {
		perror("chdir");
		return -1;
	}

	return 0;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* Execute exit/quit. */
	exit(EXIT_SUCCESS);
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	if (strchr(get_word(s->verb), '=') != 0)
		return putenv(get_word(s->verb));

	/* Sanity checks. */
	if (s == NULL || s->verb == NULL) {
		fprintf(stderr, "Error: Null simple_command_t.\n");
		return EXIT_FAILURE;
	}

	/* If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	if (strcmp(s->verb->string, "cd") == 0) {
		/* Perform redirections for cd */
		if (s->params != NULL) {
			/* Execute cd */
			int flags = O_WRONLY | O_CREAT | (s->io_flags & (IO_OUT_APPEND | IO_ERR_APPEND) ? O_APPEND : O_TRUNC);

			if (s->out != NULL) {
				int fd_out = open(get_word(s->out), flags, 0666);

				close(fd_out);
			}

			if (s->err != NULL) {
				int fd_err = open(get_word(s->err), flags, 0666);

				close(fd_err);
			}

			return shell_cd(s->params);
		} else {
			return -1;
		}
	} else if (strcmp(s->verb->string, "pwd") == 0) {
		/* Perform pwd command */
		/* Execute cd */
		int flags = O_WRONLY | O_CREAT | (s->io_flags & (IO_OUT_APPEND | IO_ERR_APPEND) ? O_APPEND : O_TRUNC);
		int fd_out;

		if (s->out != NULL)
			fd_out = open(get_word(s->out), flags, 0666);

		if (s->err != NULL) {
			int fd_err = open(get_word(s->err), flags, 0666);

			close(fd_err);
		}

		return shell_pwd(s->params, fd_out);
	} else if (strcmp(s->verb->string, "exit") == 0 || strcmp(s->verb->string, "quit") == 0) {
		return shell_exit();
	}

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	} else if (pid == 0) { // Child process
		/* Perform redirections in child */
		if (s->in != NULL) {
			int fd = open(get_word(s->in), O_RDONLY);

			if (fd == -1) {
				perror("open");
				exit(EXIT_FAILURE);
			}
			dup2(fd, STDIN_FILENO);
			close(fd);
		}

		if (s->out != NULL || s->err != NULL) {
			int flags = O_WRONLY | O_CREAT | (s->io_flags & (IO_OUT_APPEND | IO_ERR_APPEND) ? O_APPEND : O_TRUNC);
			int fd_out = -1, fd_err = -1; // Initialize with -1 to check if it was opened

			if (s->out != NULL) {
				fd_out = open(get_word(s->out), flags, 0666);
				if (fd_out == -1) {
					perror("open output");
					exit(EXIT_FAILURE);
				}
				dup2(fd_out, STDOUT_FILENO);
			}

			if (s->err != NULL) {
				if (s->out != NULL && strcmp(get_word(s->out), get_word(s->err)) == 0) {
					// If out and err have the same filename, duplicate STDOUT to STDERR
					dup2(fd_out, STDERR_FILENO);
				} else {
					// Otherwise, open a separate file descriptor for STDERR
					fd_err = open(get_word(s->err), flags, 0666);
					if (fd_err == -1) {
						perror("open error");
						exit(EXIT_FAILURE);
					}
					dup2(fd_err, STDERR_FILENO);
				}
			}

			close(fd_out);
			close(fd_err);
		}

		/* Load executable in child */
		int param_cnt = 0;
		char **params = get_argv(s, &param_cnt);

		execvp(get_word(s->verb), params);
		printf("Execution failed for '%s'\n", get_word(s->verb));

		// If execvp returns something, it failed
		exit(EXIT_FAILURE);
	} else { // Parent process
		/* Wait for child */
		int status;

		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}

	return 0;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* Execute cmd1 and cmd2 simultaneously. */

	// Fork for the first command
	pid_t pid1 = fork();

	if (pid1 == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (pid1 == 0) { // Child process for cmd1
		// Execute cmd1
		exit(parse_command(cmd1, level + 1, father));
	}

	// Fork for the second command
	pid_t pid2 = fork();

	if (pid2 == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (pid2 == 0) { // Child process for cmd2
		// Execute cmd2
		exit(parse_command(cmd2, level + 1, father));
	}

	// Wait for both children to finish
	int status1, status2;

	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);

	// Return the exit status of the second command
	return WEXITSTATUS(status2);
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	// Create a pipe
	int pipe_fds[2];

	if (pipe(pipe_fds) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	// Fork for the first command
	pid_t pid1 = fork();

	if (pid1 == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (pid1 == 0) { // Child process for cmd1
		close(pipe_fds[0]); // Close the read end of the pipe in the child

		// Redirect standard input to the read end of the pipe
		dup2(pipe_fds[1], STDOUT_FILENO);
		close(pipe_fds[1]);

		exit(parse_command(cmd1, level + 1, father));
	}

	// Fork for the second command
	pid_t pid2 = fork();

	if (pid2 == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (pid2 == 0) { // Child process for cmd2
		close(pipe_fds[1]); // Close the write end of the pipe in the child

		// Redirect standard output to the read end of the pipe
		dup2(pipe_fds[0], STDIN_FILENO);
		close(pipe_fds[0]);

		exit(parse_command(cmd2, level + 1, father));
	}

	// Close both ends of the pipe in the parent
	close(pipe_fds[0]);
	close(pipe_fds[1]);

	// Wait for both children to finish
	int status1, status2;

	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);

	// Return the exit status of the second command
	return WEXITSTATUS(status2);
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* sanity checks */

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		return parse_simple(c->scmd, level, father);
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* Execute the commands one after the other. */
		parse_command(c->cmd1, level, father);
		parse_command(c->cmd2, level, father);
		return 0;

	case OP_PARALLEL:
		/* Execute the commands simultaneously. */
		return run_in_parallel(c->cmd1, c->cmd2, level, father);

	case OP_CONDITIONAL_NZERO:
		/* Execute the second command only if the first one
		 * returns non zero.
		 */
		if (parse_command(c->cmd1, level, father) != 0)
			return parse_command(c->cmd2, level, father);
		else
			return 0;

		break;

	case OP_CONDITIONAL_ZERO:
		/* Execute the second command only if the first one
		 * returns zero.
		 */
		if (parse_command(c->cmd1, level, father) == 0)
			return parse_command(c->cmd2, level, father);
		else
			return -1;

		break;

	case OP_PIPE:
		/* Redirect the output of the first command to the
		 * input of the second.
		 */
		return run_on_pipe(c->cmd1, c->cmd2, level, father);

	default:
		return SHELL_EXIT;
	}

	return 0;
}
