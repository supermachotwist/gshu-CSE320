#include <stdlib.h>
#include <semaphore.h>
#include <string.h>

#include "client_registry.h"
#include "client.h"
#include "player.h"



/* Client Registry Definition */
struct client_registry {
	int size;  //Number of currently registered clients
	sem_t empty; //semaphore to check wheter empty or not
	sem_t mutex; //Mutex to protect client registry
	CLIENT *clients[MAX_CLIENTS];  //Array of client pointers
};

CLIENT_REGISTRY *creg_init() {
	CLIENT_REGISTRY *registry = malloc(sizeof(CLIENT_REGISTRY));
	registry->size = 0;  //Initial registry has size 0
	sem_init(&registry->empty, 0, 1); //Mutex to check whether registry is empty, 1 = empty
	sem_init(&registry->mutex, 0, 1);
	//Set all empty client objects to NULL
	for (int i = 0; i < 64; i++) {
		registry->clients[i] = NULL;
	}
	return registry;
}

void creg_fini(CLIENT_REGISTRY *cr) {
	free(cr);
}

CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd) {
	CLIENT *client = client_create(cr, fd);

	sem_wait(&cr->mutex);

	for (int i = 0; i < 64; i++) {
		if (cr->clients[i] == NULL) {  //Add client for first empty spot in registry
			cr->clients[i] = client;
			if (cr->size == 0)
				sem_wait(&cr->empty); //if registry was empty, set mutex to not empty(0)
			cr->size++;
			sem_post(&cr->mutex);
			return client;
		}
	}
	sem_post(&cr->mutex);
	return NULL;  //If failed to find empty spot
}

int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client) {

	sem_wait(&cr->mutex);

	for (int i = 0; i < 64; i++) {
		if (cr->clients[i] == client) {
			cr->clients[i] = NULL; //Set client as NULL in registry
			cr->size--;
			client_unref(client, "Unregister from client registry.");  //reduce client reference count

			if (cr->size == 0)
				sem_post(&cr->empty); //When register is empty, allow creg_wait_for_empty to continue

			sem_post(&cr->mutex);
			return 0;  //Success
		}
	}
	sem_post(&cr->mutex);
	return -1;  //Failure
}

CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user) {

	sem_wait(&cr->mutex);

	for (int i = 0; i < 64; i++) {
		if (cr->clients[i] != NULL) {
			if (client_get_player(cr->clients[i]) != NULL){//Check if player is logged in
				/* Check whether player's name equals searched user */
				if (!strcmp(player_get_name(client_get_player(cr->clients[i])), user)) {
					sem_post(&cr->mutex);
					return cr->clients[i];  //User found
				}
			}
		}
	}
	sem_post(&cr->mutex);
	return NULL;  //User not found
}

PLAYER **creg_all_players(CLIENT_REGISTRY *cr) {
	/* List to return */
	PLAYER **players = malloc(0);
	int playercount = 0; //Count number of players to help with memory allocation

	sem_wait(&cr->mutex);

	for (int i = 0; i < 64; i++) {
		if (cr->clients[i] != NULL) {
			if (client_get_player(cr->clients[i]) != NULL) {  //Check if client is logged in
				playercount++;
				players = realloc(players, playercount * 8);
				player_ref(client_get_player(cr->clients[i]),"Increment player reference when Users is called for returned list.");
				*(players + playercount - 1) = client_get_player(cr->clients[i]); //add player to end of list
			}
		}
	}
	/* Add NULL pointer to end of array */
	players = realloc(players, (playercount+1)*8);
	*(players + playercount) = NULL;
	sem_post(&cr->mutex);
	return players;
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
	sem_wait(&cr->empty);  //Wait for empty register
	sem_post(&cr->empty);
}

void creg_shutdown_all(CLIENT_REGISTRY *cr) {

	sem_wait(&cr->mutex);

	for (int i = 0; i < 64; i++) {
		if(cr->clients[i] != NULL) {
			shutdown(client_get_fd(cr->clients[i]),SHUT_RDWR);
		}
	}
	sem_post(&cr->mutex);
}