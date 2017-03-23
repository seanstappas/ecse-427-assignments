#define _XOPEN_SOURCE 700 // getline
#define _BSD_SOURCE // strsep
#define MAX_JOBS 1000 // ASK: How many concurrent jobs?
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

pid_t children_pgid, foreground_pid, background_pids[MAX_JOBS];
int job_nb = 0;
char *background_commands[MAX_JOBS];

static void handlerSIGINT(int sig) {
	if (sig == SIGINT) { // Ctrl-C
		if (foreground_pid > 0) {
			if (kill(foreground_pid, SIGKILL) == 0) { // ASK: Kill all child processes, or just foreground process?
				printf("PID %d killed\n", foreground_pid);
			} else {
				perror("Kill failed");
			}
			foreground_pid = 0;
		}
	}
}

// static void handlerSIGCHLD(int sig) {
// 	pid_t pid = getpid();
// 	printf("SIGCHLD fired for %d\n", pid);
// 	if (sig == SIGCHLD) { // Child finished
// 		int status;
// 		pid_t pid = wait(&status);
// 		if (status == 0 && pid != -1) {
// 			for (int i = 0; i < MAX_JOBS; i++) {
// 				if (background_pids[i] == pid) {
// 					background_pids[i] = 0;
// 					background_commands[i] = NULL;
// 					return;
// 				}
// 			}
// 		}
// 	}
// }

//
// This code is given for illustration purposes. You need not include or follow this
// strictly. Feel free to writer better or bug free code. This example code block does not
// worry about deallocating memory. You need to ensure memory is allocated and deallocated
// properly so that your shell works without leaking memory.
//
int getcmd(char *prompt, char *args[], int *background, int *redirection, int *piping)
{
	int length, i = 0;
	char *token, *loc;
	char *line = NULL;
	size_t linecap = 0;
	printf("%s", prompt);
	length = getline(&line, &linecap, stdin);
	if (length <= 0) {
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
		*redirection = 1;
		*loc = ' ';
	} else
		*redirection = 0;
	// Check if piping is specified..
	if ((loc = strchr(line, '|')) != NULL) {
		*piping = 1;
		*loc = ' ';
	} else
		*piping = 0;
	while ((token = strsep(&line, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
	}
	args[i] = NULL;
	return i;
}
int main(void)
{
	if (signal(SIGINT, handlerSIGINT) == SIG_ERR) {
		printf("ERROR: Could not bind SIGINT signal handler\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
		printf("ERROR: Could not bind SIGTSTP signal handler\n");
		exit(EXIT_FAILURE);
	}

	// if (signal(SIGCHLD, handlerSIGCHLD) == SIG_ERR) {
	// 	printf("ERROR: Could not bind SIGCHLD signal handler\n");
	// 	exit(EXIT_FAILURE);
	// }

	char *args[MAX_ARGS];
	int bg, redir, pipe;
	while (1) {
		bg = 0;
		int cnt = getcmd("\n>> ", args, &bg, &redir, &pipe);
		for (int i = 0; i < MAX_ARGS; i++) {
			if (args[i] == NULL) {
				break;
			}
			printf("Argument %d: %s\n", i, args[i]);
		}
		if (cnt > 0) {
			if (strcmp(args[0], "cd") == 0) {
				if (cnt == 1)
					printf("No directory specified.\n");
				else
					chdir(args[1]);
			} else if (strcmp(args[0], "pwd") == 0) {
				char cwd[1024];
				getcwd(cwd, sizeof(cwd));
				printf("%s", cwd);
			} else if (strcmp(args[0], "exit") == 0) {
				exit(EXIT_SUCCESS);
			} else if (strcmp(args[0], "fg") == 0) {
				if (cnt == 1)
					printf("No background job specified.\n");
				else {
					int selected_job = atoi(args[1]);
					if (selected_job < 1 || background_pids[selected_job - 1] == 0) {
						printf("Invalid job number.\n");
					} else {
						foreground_pid = background_pids[selected_job - 1];
						int status;
						waitpid(foreground_pid, &status, 0); // Wait for child
						foreground_pid = 0;
						background_pids[selected_job - 1] = 0;
						background_commands[selected_job - 1] = NULL;
						if (status == 0) {
							// Success
						} else {
							if (WIFEXITED(status)) {
								int exit_status = WEXITSTATUS(status);
								printf("Child terminated normally (exit status %d)\n", exit_status);
							}
							if (WIFSIGNALED(status)) {
								int signal_number = WTERMSIG(status);
								char *signal = strsignal(signal_number);
								printf("Child terminated by signal #%d (%s)\n", signal_number, signal);
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
			} else if (strcmp(args[0], "jobs") == 0) {
				printf("Job # |  PID  |  Command\n"); // ASK: What to output exactly? What to output when 0 jobs?
				printf("--------------------------\n");
				for (int i = 0; i < MAX_JOBS; i++) {
					if (background_pids[i] != 0) {
						printf("%5d | %5d | %s\n", i + 1, background_pids[i], background_commands[i]);
					}
				}
			} else {
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
					if (redir == 1) {
						if (cnt < 2) {
							printf("No output file specified\n");
							exit(EXIT_FAILURE);
						}
						close(1);
						char* file;
						for (int i = 0; i < MAX_ARGS; i++) {
							if (args[i] == NULL) {
								open(args[i-1], O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // emulating linux behaviour
								args[i-1] = NULL;
								break;
							}
						}
					}
					execvp(args[0], args);
					perror("execvp failed");
					exit(EXIT_FAILURE);
				} else {
					// In parent
					if (bg == 0) { // foreground process
						foreground_pid = pid;
						int status;
						if (waitpid(pid, &status, 0) == pid) { // Wait for child
							foreground_pid = 0;
							if (status == 0) {
								// Success
							} else {
								("Error while waiting for child (parent)\n");
							}
						} else {
							foreground_pid = 0;
							perror("Error while waiting for child (parent-perror)");
						}
					} else {
						// Background process
						background_pids[job_nb] = pid;
						for (int i = 1; i < cnt; i++) {
							sprintf(args[0], "%s %s", args[0], args[i]); // Append command strings (for job listing)
						}
						background_commands[job_nb] = args[0];
						job_nb = (job_nb + 1) % MAX_JOBS;
					}
				}
			}
		}
	}
}