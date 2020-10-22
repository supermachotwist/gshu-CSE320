/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* Directory into which the daemon log files are to be placed. */
#define LOGFILE_DIR "logs"

/* Maximum number of versions of log files to be retained during rotation. */
#define LOG_VERSIONS 7

/* Directory in which "daemons" executables are found. */
#define DAEMONS_DIR "daemons"

/* String that is the name of the PATH environment variable. */
#define PATH_ENV_VAR "PATH"

/*
 * Amount of time (in seconds) to wait for child process, either for the synchronization
 * message on startup or for termination to occur on shutdown after having sent SIGTERM.
 */
#define CHILD_TIMEOUT 1

/*
 * File descriptor on which a daemon expects to write the one-byte synchronization
 * message on startup.  This will need to be redirected to the output side of a pipe.
 */
#define SYNC_FD 3

/*
 * Possible statuses for a daemon.  See the assignment document for more information.
 */
enum daemon_status {
    status_unknown, status_inactive, status_starting, status_active,
    status_stopping, status_exited, status_crashed
};

/*
 * EVENT FUNCTIONS THAT YOU MUST CALL FROM WITHIN YOUR MASTER PROCESS
 * See the assignment document for further information.
 */

void sf_init(void);
void sf_fini(void);
void sf_register(char *daemon_name, char *cmd);
void sf_unregister(char *daemon_name);
void sf_start(char *daemon_name);
void sf_active(char *daemon_name, pid_t pid);
void sf_stop(char *daemon_name, pid_t pid);
void sf_reset(char *daemon_name);
void sf_kill(char *daemon_name, pid_t pid);
void sf_logrotate(char *daemon_name);
void sf_term(char *daemon_name, pid_t pid, int exit_status);
void sf_crash(char *daemon_name, pid_t pid, int signal);
void sf_error(char *reason);

/*
 * FUNCTIONS YOU ARE TO IMPLEMENT
 * See the assignment document for further information.
 */

void run_cli(FILE *in, FILE *out);
