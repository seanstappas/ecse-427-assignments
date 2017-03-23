#define _XOPEN_SOURCE 700 // getline
#define _BSD_SOURCE // strsep
#define MAX_JOBS 1000
#define MAX_ARGS 20

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

pid_t foreground_pid, background_pids[MAX_JOBS];
int job_nb = 0;
char *background_commands[MAX_JOBS];


/*
 * Handler for the SIGINT signal (Ctrl-C).
 */

static void handlerSIGINT(int sig) {
	if (sig == SIGINT) {
		if (foreground_pid > 0) {
			if (kill(foreground_pid, SIGTERM) != 0) { // Terminate foreground process
				perror("Terminate failed");
			}
			foreground_pid = 0;
		}
	}
}

/*
 * Handler for the SIGCHLD signal (called when child process stops or terminates).
 */

static void handlerSIGCHLD(int sig) {
	if (sig == SIGCHLD) {
		int status = 0;
		pid_t pid = waitpid(-1, &status, WNOHANG); // Check if process was recently stopped
		if (status == 0 && pid != -1) {
			for (int i = 0; i < MAX_JOBS; i++) {
				if (background_pids[i] == pid) { // Check if background process
					background_pids[i] = 0;      // Update arrays (for 'jobs' command)
					free(background_commands[i]);
					background_commands[i] = NULL;
					return;
				}
			}
		}
	}
}

/*
 * Free allocated memory for the command arguments.
 */

void freeCommandArguments(char *args[], char *args2[]) {
	for (int i = 0; i < MAX_ARGS; i++) {
		if (args[i] == NULL)
			break;
		free(args[i]);
	}
	for (int i = 0; i < MAX_ARGS; i++) {
		if (args2[i] == NULL)
			break;
		free(args2[i]);
	}
}

/*
 * Free all allocated memory.
 */
void freeMemory(char *args[], char *args2[]) {
	freeCommandArguments(args, args2);
	for (int i = 0; i < MAX_JOBS; i++) {
		if (background_commands[i] != NULL)
			free(background_commands[i]);
	}
}

/*
 * Get command from prompt 
 * --------------------
 *
 *  prompt:     prompt string to print to user
 *  args:       array of arguments
 *  background: flag to indicate a background command
 *  redir:      flag to indicate a redirection command
 *  piping:     flag to indicate a pipe command
 *  args2:      second array of arguments (used for piping)
 *
 *  returns: the number of arguments in the command
 */

int getcmd(char *prompt, char *args[], int *background, int *redir, int *piping, char *args2[])
{
	int i = 0, k = 0;
	char *token, *loc;
	char *line = NULL;
	size_t linecap = 0;
	printf("%s", prompt);
	ssize_t length = getline(&line, &linecap, stdin);
	if (length <= 0) {
		free(line);
		exit(EXIT_FAILURE);
	}
	// Check if background is specified..
	if ((loc = strchr(line, '&')) != NULL) {
		*background = 1;
		*loc = ' ';
	} else
		*background = 0;
	// Check if output redirection is specified..
	if ((loc = strchr(line, '>')) != NULL) {
		*redir = 1;
		*loc = ' ';
	} else
		*redir = 0;

	*piping = 0;
	char *line_copy = line;
	while ((token = strsep(&line_copy, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0) {
			if (strcmp(token, "|") == 0) {
				*piping = 1;
			} else {
				if (*piping == 1) {
					args2[k++] = strdup(token); // Second command (for piping)
				} else {
					args[i++] = strdup(token); // First command
				}
			}
		}
	}
	free(line);
	args[i] = NULL;
	args2[k] = NULL;
	return i + k;
}

/*
 * Execute command given by args and args2
 * --------------------
 *
 *  cnt:    number of command arguments
 *  args:   array of arguments
 *  bg:     flag to indicate a background command
 *  redir:  flag to indicate a redirection command
 *  piping: flag to indicate a pipe command
 *  args2:  second array of arguments (used for piping)
 *
 */

void executeCommand(int cnt, char *args[], int bg, int redir, int piping, char *args2[]) {
	// BUILT-IN COMMANDS
	if (strcmp(args[0], "cd") == 0) { // Change directory
		if (cnt == 1)
			printf("No directory specified.\n");
		else
			chdir(args[1]);
	} else if (strcmp(args[0], "pwd") == 0) { // Present working directory
		char cwd[1024];
		getcwd(cwd, sizeof(cwd));
		printf("%s", cwd);
	} else if (strcmp(args[0], "exit") == 0) { // Exit
		freeMemory(args, args2);
		exit(EXIT_SUCCESS);
	} else if (strcmp(args[0], "fg") == 0) { // Foreground a job
		if (cnt == 1)
			printf("No background job specified.\n");
		else {
			int selected_job = atoi(args[1]);
			if (selected_job < 1 || background_pids[selected_job - 1] == 0) {
				printf("Invalid job number.\n");
			} else {
				foreground_pid = background_pids[selected_job - 1];
				int status = 0;
				waitpid(foreground_pid, &status, 0); // Wait for child
				foreground_pid = 0;
				background_pids[selected_job - 1] = 0;
				free(background_commands[selected_job - 1]);
				background_commands[selected_job - 1] = NULL;
				if (status != 0) {
					if (WIFEXITED(status)) {
						int exit_status = WEXITSTATUS(status);
						printf("Child terminated normally (exit status %d)\n", exit_status);
					}
					if (WIFSIGNALED(status)) {
						int signal_number = WTERMSIG(status);
						char *signal = strsignal(signal_number);
						// Child terminated by signal (not printed to enhance user experience)
						if (WCOREDUMP(status)) {
							printf("Child produced core dump\n");
						}
					}
					if (WIFSTOPPED(status)) {
						int signal_number = 0;
						printf("Child stopped by delivery of signal #%d\n", signal_number);
					}
					if (WIFCONTINUED(status)) {
						printf("Child process was resumed by delivery of SIGCONT\n");
					}
				}
			}
		}
	} else if (strcmp(args[0], "jobs") == 0) { // List background jobs
		printf("Job # |  PID  |  Command\n");
		printf("--------------------------\n");
		for (int i = 0; i < MAX_JOBS; i++) {
			if (background_pids[i] != 0) {
				printf("%5d | %5d | %s\n", i + 1, background_pids[i], background_commands[i]);
			}
		}
	} else {
		// FORK COMMANDS
		pid_t pid = fork();
		if (pid == -1) {
			perror("Failed to fork");
			exit(EXIT_FAILURE);
		} else if (pid == 0) {
			// In child
			if (signal(SIGINT, SIG_IGN) == SIG_ERR) { // Ignore Ctrl-C in children
				printf("ERROR: Could not bind SIGINT signal handler\n");
				exit(EXIT_FAILURE);
			}
			// Redirect
			if (redir == 1) {
				if (cnt < 2) {
					printf("No output file specified\n");
					exit(EXIT_FAILURE);
				}
				close(1);
				for (int i = 0; i < MAX_ARGS; i++) {
					if (args[i] == NULL) {
						open(args[i - 1], O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // Default Linux permissions
						args[i - 1] = NULL;
						break;
					}
				}
			}
			// Pipe
			if (piping == 1) {
				if (cnt < 2) {
					printf("No second command specified\n");
					exit(EXIT_FAILURE);
				}
				int fd[2];
				if (pipe(fd) == -1) {
					perror("Error while piping");
					exit(EXIT_FAILURE);
				}
				pid_t pid2 = fork(); // Create second child for piping
				if (pid2 == -1) {
					perror("Failed to fork");
					exit(EXIT_FAILURE);
				} else if (pid2 == 0) {
					// In second child
					if (dup2(fd[1], STDOUT_FILENO) == -1) { // Write to STDOUT
						perror("dup2 on STDOUT failed");
					}
					close(fd[0]);
					close(fd[1]);
					execvp(args[0], args);
					perror("execvp failed");
					exit(EXIT_FAILURE);
				} else {
					// In original child
					if (dup2(fd[0], STDIN_FILENO) == -1) { // Read from STDIN
						perror("dup2 on STDIN failed");
					}
					close(fd[0]);
					close(fd[1]);
					int status;
					if (waitpid(pid2, &status, 0) == pid2) { // Wait for child
						if (status != 0) {
							// Error while waiting for second child (not printed to enhance user experience)
						}
					} else {
						perror("Error while waiting for second child");
					}
					execvp(args2[0], args2);
				}
			}
			else {
				execvp(args[0], args);
			}
			freeCommandArguments(args, args2);
			perror("execvp failed");
			exit(EXIT_FAILURE);
		} else {
			// In parent
			if (bg == 0) { // foreground process
				foreground_pid = pid;
				int status = 0;
				if (waitpid(pid, &status, 0) == pid) { // Wait for child
					foreground_pid = 0;
					if (status != 0) {
						// Error while waiting for child (not printed to enhance user experience)
					}
				} else {
					foreground_pid = 0;
					perror("Error while waiting for child");
				}
			} else {
				// Background process
				background_pids[job_nb] = pid;
				size_t total_size = 0;
				for (int i = 0; i < cnt; i++) {
					total_size += sizeof(args[i]);
				}
				char *command  = (char *) malloc(total_size);
				strcpy(command, args[0]);
				for (int i = 1; i < cnt; i++) {
					sprintf(command, "%s %s", command, args[i]); // Append command strings (for job listing)
				}
				background_commands[job_nb] = command;
				job_nb = (job_nb + 1) % MAX_JOBS;
			}
		}
	}
}

int main(void)
{
	if (signal(SIGINT, handlerSIGINT) == SIG_ERR) { // Handle Ctrl-C interrupt
		printf("ERROR: Could not bind SIGINT signal handler\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) { // Ignore Ctrl-Z interrupt
		printf("ERROR: Could not bind SIGTSTP signal handler\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGCHLD, handlerSIGCHLD) == SIG_ERR) { // Handle child interrupt
		printf("ERROR: Could not bind SIGCHLD signal handler\n");
		exit(EXIT_FAILURE);
	}

	char *args[MAX_ARGS], *args2[MAX_ARGS];
	int bg, redir, piping;
	while (1) {
		bg = 0;
		int cnt = getcmd("\n>> ", args, &bg, &redir, &piping, args2);
		if (cnt > 0) {
			executeCommand(cnt, args, bg, redir, piping, args2);
		}
		freeCommandArguments(args, args2);
	}
}