#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include "player_registry.h"

struct player {
	char *name;
	int rating;
	int ref_count;
	PLAYER *next;
	sem_t mutex;
};

struct player_registry {
	sem_t mutex;
	PLAYER *head;  //Store store players as a linked list
};

PLAYER_REGISTRY *preg_init(void) {
	PLAYER_REGISTRY *registry = malloc(sizeof(PLAYER_REGISTRY));
	sem_init(&registry->mutex,0,1);
	registry->head = NULL;
	return registry;
}

void preg_fini(PLAYER_REGISTRY *preg) {
	free(preg);
}

PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name) {
	/* Protect playe registry */
	sem_wait(&preg->mutex);
	PLAYER *current = preg->head;
	while (current != NULL) {
		/* Check if player already exists */
		if (!strcmp(player_get_name(current), name)) {  //If player already exists
			player_ref(current, "because of return new reference to existing player");
			return current;  //Return existing player
		}
		current = current->next; //move to next in linked list
	}

	/* Create new player to add  to registry */
	PLAYER *player = player_create(name);
	current = player;
	sem_post(&preg->mutex);
	player_ref(player, "for addition into player registry");
	return player;
}
