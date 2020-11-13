/*
 * Legion: Command-line interface
 */

#include "legion.h"
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>

#define MAXARGS 128
#define HELPMSG "Available commands:\n" \
				"help (0 args) Print this help message\n" \
				"quit (0 args) Quit the program\n" \
				"register (2 args) Register a daemon\n" \
				"unregister (1 args) Unregister a daemon\n" \
				"status (1 args) Show the status of a daemon\n" \
				"status-all (0 args) Show the status of all daemons\n" \
				"start (1 args) Start a daemon\n" \
				"stop (1 args) Stop a daemon\n" \
				"logrotate (1 args) Rotate log files for a daemon\n"

extern char **environ;

volatile sig_atomic_t flag; //Flag to share between and main program
volatile pid_t spid = 0;  //Shared child process to kill

struct registered_daemons { //doubly linked list for holding registered daemons
	char name[50];
	char exe[50];
	pid_t pid;
	char status[20];
	char **argv;
	struct registered_daemons *prev;
	struct registered_daemons *next;
} head;

/* TODO: Install SIGALRM handler for parent process */
void sigalrm_handler(int sig) {
	kill(spid, SIGKILL);  //Kill the child process
	waitpid(spid, NULL, 0);  //Reap single child process
	flag = 1;  //Set flag to indicate sigalrm was caught
	return; //Return to main program
}
/* TODO: Install SIGTERM handler for child process */
/* TODO: Install SIGCHLD handler for parent process */
void sigchld_handler(int sig) {
	int olderrno = errno;
	int status = 0;
	pid_t cpid;

	/* Reap as many zombie processes as possible */
	while ((cpid = waitpid(-1, &status, 0)) > 0) {
		if (WIFEXITED(status)) {
			flag = 2;  //Set flag to show child process exited normally
			spid = cpid;  //Share child that crashed with main program
		}
		else {
			flag = 3;  //Set flag to show child process crashed
			spid = cpid;  //Share child that crashed with main program
		}
	}

	if (errno != ECHILD) {
		sf_error("wait error");
	}

	errno = olderrno;
	return;
}
/* TODO: Install SIGINT handler for parent process */

/* Find registered daemon using reigstered name */
struct registered_daemons* find_daemon(char *name) {
	struct registered_daemons *current = head.next;
	while (current != NULL) {
		/* Find daemon in linked list and return */
		if (!strcmp(current->name, name)) {
			return current;  //Return to cmdline
		}
		current = current->next;
	}
	return NULL; //Return to cmdline
}

/* Find registered daemon using pid */
struct registered_daemons* find_daemon_pid(pid_t pid) {
	struct registered_daemons *current = head.next;
	while (current != NULL) {
		/* find daemon in linked list and return */
		if (current->pid == pid) {
			return current;  //Return to cmdline
		}
	}
	return NULL;
}


/*parseline  - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) { 	//Return argument count
	char *delim;			/* Points to the first space delimiter */
	int argc;				/* Number of arguments */
	int bufsize = MAXARGS;  /*Curent size of buffer */

	while (*buf && (*buf == ' ')) { /* Ignore leading spaces */
		buf++;
	}

	delim = buf;
	argc = 0;

	/* Build the argv array with single quote delimiter */
	while (1){
		if (*delim == ' ') {
			argv[argc++] = buf;
			*delim = '\0';	/* End string of current argument */
			buf = ++delim;
			while (*buf && (*buf == ' ')) { /* Ignore spaces */
				buf++;
				delim++;
			}
		}
		else if(*delim == '\n') {
			if (*(delim-1) != ' ' && *(delim-1) != '\0') {
				argv[argc++] = buf;
				*delim = '\0';	/* End string of current argument */
			}
			break;
		}
		else if (*delim == '\'') {

			if (*(delim-1) != ' ' && *(delim-1) != '\0') {
				argv[argc++] = buf;
				*delim = '\0';  /* End string of current argument */
				buf = ++delim;
				while (*buf && (*buf == ' ')) { /* Ignore spaces */
					buf++;
				}
			}
			else
				buf++;					/* Exclude the single quote from argument */

			if ((delim = strchr(buf, '\''))) {  /* Check for end quotes */
				argv[argc++] = buf;
				*delim = '\0';
				buf = ++delim;
				while (*buf && (*buf == ' ')) { /* Ignore spaces */
					buf++;
					delim++;
				}
			}
			else {  /* If there is no end quotes, rest of cmd line is one argument */
				argv[argc++] = buf;
				buf[strlen(buf) - 1] = '\0';
				break;
			}
		}
		else {		/* Loop till first space or single quote */
			delim++;
		}

		/* Dynamically allocate argv */
		if (argc >= bufsize) {
			bufsize += MAXARGS;
			argv = realloc(argv, bufsize * sizeof(char*));
			if (!argv) {
				sf_error("Parseline: Memory reallocation error");
				return -1;
			}
		}
	}

	argv[argc] = NULL; //Set final arg as null

	return argc;
}

void eval(char *cmdline) {
	char **argv = malloc(MAXARGS * sizeof(char*)); /* Argument list execve() */
	int argc = parseline(cmdline, argv);

	/* If input is empty or gives realloc errer return (Do not exit) */
	if (argv[0] == NULL || argc == -1) {
		return;
	}

	/* Help Command */
	else if (!strcmp(argv[0],"help")) {
		printf(HELPMSG);
		return;
	}

	/* Quit Command */
	else if (!strcmp(argv[0], "quit")) {
		exit(0);
	}

	/* Register Command */
	else if (!strcmp(argv[0], "register")) {

		if (argc == 1 || argc == 2) {
			sf_error("Register: Please enter required arguments");
			printf(HELPMSG);
			return; //Return to cmdline
		}
		else {
			struct registered_daemons *sp = find_daemon(argv[1]);
			if (sp != NULL) {
				sf_error("Register: Daemon name already taken");
				return;
			}
			/* Initialize new struct for registered daemon */
			sp = malloc(sizeof(struct registered_daemons));
			strcpy(sp->name, argv[1]); //Set name given by user
			strcpy(sp->exe, argv[2]);  //Set runnable exe
			sp->pid = 0;  //Process id is 0 if inactive
			strcpy(sp->status, "inactive");

			/* Add daemon into doubly linked list */
			sp->next = head.next;	//Set current daemon next to next of sentinel
			sp->prev = &head;		//Set current daemon prev to sentinel head
			head.next = sp;

			/* Set argument vector for daemon */
			sp->argv = &argv[2];

			sf_register(sp->name, *(sp->argv));
			return; //Return to cmdline
		}
	}

	/* Unregister Command */
	else if(!strcmp(argv[0], "unregister")) {
		if (argc == 1) {
			printf("Unregister: Please enter required arguments\n");
			printf(HELPMSG);
			return; //Return to cmdline
		}

		else if (argc == 2) {
			struct registered_daemons *current = head.next;
			while (current != NULL) {
				/* Find daemon in linked list and check for inactive status */
				if (!strcmp(current->name, argv[1]) && !strcmp(current->status, "inactive")) {
					/* Remove daemon from linked list */
					current->prev->next = current->next;
					sf_unregister(current->name);
					free(current);
					return;  //Return to cmdline
				}
				current = current->next;
			}
			/* If daemon not found or status other than inactive */
			sf_error("Unregister: Daemon not found or not inactive");
			return; //Return to cmdline
		}

		else {
			printf("Unregister: Too many arguments\n");
			printf(HELPMSG);
			return;  //Return to cmdline
		}

	}

	/* Status Command */
	else if(!strcmp(argv[0], "status")) {
		if (argc == 1) {
			printf("Unregister: Please enter required arguments\n");
			printf(HELPMSG);
			return; //Return to cmdline
		}
		else if (argc == 2) {
			struct registered_daemons *sp = find_daemon(argv[1]);
			if (sp == NULL){
				sf_error("Status:Daemon not found");
				return;
			}
			printf("%s\t%d\t%s\n", sp->name, sp->pid, sp->status);
		}
		else {
			printf("Status: Too many arguments\n");
			printf(HELPMSG);
			return;  //Return to cmdline
		}
	}

	/* Status-All Command */
	else if(!strcmp(argv[0], "status-all")) {
		if (argc == 1) {
			struct registered_daemons *current = head.next;
			while (current != NULL) {
				printf("%s\t%d\t%s\n", current->name, current->pid, current->status);
				current = current->next;
			}
		}
		else {
			printf("Status-All: Too many arguments\n");
			printf(HELPMSG);
			return;  //Return to cmdline
		}
		return; //Return to cmdline
	}

	/* Start Command */
	else if(!strcmp(argv[0], "start")) {
		if (argc == 1) {
			printf("Start: Please enter required arguments\n");
			printf(HELPMSG);
			return; //Return to cmdline
		}
		else if (argc == 2) {
			struct registered_daemons *sp = find_daemon(argv[1]);
			if (sp != NULL) {
				if (strcmp(sp->status, "inactive")){
					sf_error("Start:Daemon needs to be inactive");
					return;
				}
				strcpy(sp->status, "starting");

				int pipefd[2]; //pipefd[0] = read, pipefd[1] = write
				pid_t cpid;


				if (pipe(pipefd) == -1) {
					sf_error("Start:Pipe Error");
					return;
				}

				cpid = fork();
				if (cpid == -1) {
					sf_error("Start:Fork Error");
					strcpy(sp->status, "inactive");
					return;
				}

				/* Child process */
				if (cpid == 0) {
					setenv("PATH", strcat(strcat(DAEMONS_DIR,":"), getenv("PATH")),1);  //Prepend DAEMON_DIR to existing value
					setpgid(0,0);  //Differentiate process group from parent process

					/* Redirect output side of pipe to new file descriptor */
					if (dup2(pipefd[1],SYNC_FD) == -1) {
						sf_error(strerror(errno));
						return;
					}
					/* Send one bit synchronization message */
					char buf = 'a';
					write(pipefd[1],&buf, 1);
					/* If error running executable */
					if (execvpe(sp->exe, sp->argv, environ) == -1) {
						sf_error(strerror(errno));
						return;
					}

				}

				/* Parent process */
				else {
					/* Block all signals except SIGALRM till status is set to active */
					sigset_t mask, prev_mask;
					sigemptyset(&mask);
					sigfillset(&mask);
					sigdelset(&mask,SIGALRM);
					sigprocmask(SIG_BLOCK, &mask, &prev_mask);

					char buf;
					spid = cpid;  //Set shared pid to child pid
					/* Send SIGALRM to parent process in 1 second */
					alarm(CHILD_TIMEOUT);

					/* Read Error */
					if (!read(pipefd[0], &buf, 1) || flag == 1) {
						strcpy(sp->status, "inactive");
						sf_error("Start: Parent read error");
						flag = 0;
						/* Restore signal mask before returning */
						sigprocmask(SIG_SETMASK, &prev_mask, NULL);
						return; //Return to cmdline
					}
					/* Check for synchronization message from child process */
					else {
						alarm(0);  //Cancel previous alarm
						spid = 0; //Reset shared pid
						sp->pid = cpid;  //Store pid of child process
						strcpy(sp->status, "active");
						close(pipefd[0]);
						close(pipefd[1]);
						/* Restore signal mask after status is active */
						sigprocmask(SIG_SETMASK, &prev_mask, NULL);
						return; //Return to cmdline
					}
				}


			}
			else {
				sf_error("Start:Daemon not found");
				return;
			}
		}
		/* When daemon is not found */
		else {
			printf("Start: Too many arguments\n");
			printf(HELPMSG);
			return;  //Return to cmdline
		}
	}

	else if(!strcmp(argv[0], "stop")) {
		struct registered_daemons *sp = find_daemon(argv[1]);
		if (sp != NULL) {
			/* Set deamon status to inactive if exited or crashed */
			if (!strcmp(sp->status, "exited") || !strcmp(sp->status, "crashed")) {
				strcpy(sp->status, "inactive");
				sp->pid = 0;
				return; //Return to cmdline
			}

			/* Check if status is anything other than active */
			if (!strcmp(sp->status, "active")) {
				strcpy(sp->status, "stopping");
				kill(sp->pid, SIGTERM);
				spid = sp->pid;
				alarm(CHILD_TIMEOUT);
				sigset_t mask;
				sigemptyset(&mask);
				sigfillset(&mask);
				sigdelset(&mask, SIGCHLD);
				sigdelset(&mask, SIGINT);
				sigsuspend(&mask);

				if (flag == 1) {  //If sigalrm was caught
					sf_error("Stop: SIGALRM error");
					flag = 0;
					strcpy(sp->status, "inactive");
					return; //Return to cmdline
				}
				else {  //If sigchld was caught
					alarm(0);  //Turn off alarm
					flag = 0;
					strcpy(sp->status, "inactive");
					return;
				}
			}
			else {
				sf_error("Stop: Unable to stop non-active daemon");
				return; //Return to cmdline
			}
		}

		/* When deamon not found */
		else {
			sf_error("Stop:Daemon not found");
			return;
		}
	}

	else {
		printf(HELPMSG);
		return;
	}

}

/* Returns NULL if end of file or error encountered */
char *read_line(void) {
  char *line = NULL;
  size_t bufsize = 0; // have getline allocate a buffer for us

  	if (getline(&line, &bufsize, stdin) == -1){
    	if (feof(stdin)) {
    		sf_error("End of file exception");
      		return NULL;  // We recieved an EOF
   		 }
   		else  {
      		sf_error("readline error");
      		return NULL;
    	}
  	}

  return line;
}

void run_cli(FILE *in, FILE *out) {
	/* Install signal handlers */
	if (signal(SIGALRM, sigalrm_handler) == SIG_ERR || signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
		sf_error("Signal Error");
	}

	char *cmdline;

	/* Initialize header for registered linked list */
	head.next = NULL;
	head.prev = NULL;

    while(1) {

    	printf("legion> ");
    	cmdline = read_line();

    	/* Evaluate */
    	eval(cmdline);

    	free(cmdline); //Flush command line input

    	/* When a child process exits randomly*/
	    if (flag == 2) {
	    	struct registered_daemons *sp = find_daemon_pid(spid);
	    	/* Set registered_daemons properties correctly */
	    	sp->pid = 0;
	    	strcpy(sp->status, "exited");
	    	flag = 0;
	    }
	    /* When a child process crashes */
	    if (flag == 3) {
	    	struct registered_daemons *sp = find_daemon_pid(spid);
	    	/* Set registered_daemons properties correctly */
	    	sp->pid = 0;
	    	strcpy(sp->status, "crashed");
	    	flag = 0;
	    }
    }
}

