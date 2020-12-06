#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>

#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);


/* SIGHUP Handler */
void sighup_handler (int signum) {
	terminate(EXIT_SUCCESS);;
}


int open_listenfd(char *port) {
	struct addrinfo hints, *listp, *p;
	int listenfd, optval=1;

	/* Get a list of potential server addresses */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM; /* Accept connections */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
	hints.ai_flags |= AI_NUMERICSERV; /* ... using port number */
	getaddrinfo(NULL, port, &hints, &listp);

	/* Walk the list for one that we can bind to */
	for (p = listp; p; p = p->ai_next) {
		/* Create a socket descriptor */
		if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue; /* Socket failed, try the next */

		/* Eliminates "Address already in use" error from bind */
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

		/* Bind the descriptor to the address */
		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
			break; /* Success */
		close(listenfd); /* Bind failed, try the next */
	}

	/* Clean up */
	freeaddrinfo(listp);
	if (!p) /* No address worked */
		return -1;

	/* Make it a listening socket ready to accept connection requests */
	if (listen(listenfd, 1024) < 0) {
		close(listenfd);
		return -1;
	}
	return listenfd;
}

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){
	// Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
   	char *port = NULL;
   	int c;

   	opterr = 0;

   	while ((c = getopt(argc, argv, "p:")) != -1)
   		switch (c) {
   			case 'p':
   				port = optarg;
   				break;
   			case '?':
   				if (optopt == 'p')
		          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
		        else if (isprint (optopt))
		          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
		        else
		          fprintf (stderr,
		                   "Unknown option character `\\x%x'.\n",
		                   optopt);
		        return 1;
		    default:
		        abort ();
   		}



    // Perform required initializations of the client_registry and
    // player_registry.
    client_registry = creg_init();
    player_registry = preg_init();


    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    struct sigaction new_action;

    /* Set up structure for SIGHUP handler */
    new_action.sa_handler = sighup_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;


    /* Install SIGHUP handler */
    sigaction(SIGHUP, &new_action, NULL);

    /* Install SIGPIPE handler */
    //new_action.sa_handler = sigpipe_handler;
    //sigaction(SIGPIPE, &new_action, NULL);

    /* server loop */
    pthread_t tid;  //thread id
    int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    listenfd = open_listenfd(port);
    while(1) {
      int *connfd = malloc(4);
    	clientlen = sizeof(struct sockaddr_storage);
    	*connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
    	pthread_create(&tid, NULL, jeux_client_service, connfd);
    }

    terminate(EXIT_SUCCESS);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);

    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}
