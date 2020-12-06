#include <stdlib.h>
#include <semaphore.h>

#include "client_registry.h"
#include "invitation.h"
#include "game.h"


struct invitation {
	CLIENT *source;
	CLIENT *target;
	int ref_count;
	int state;
	GAME_ROLE source_role;
	GAME_ROLE target_role;
	GAME *game;
	sem_t mutex;
};

INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role) {
	/* Check if source and target clients are the same */
	if (source == target)
		return NULL;

	/* Create and initialize invitation */
	INVITATION *inv = malloc(sizeof(INVITATION));
	inv->source = source;
	inv->target = target;
	inv->source_role = source_role;
	inv->target_role = target_role;
	inv->state = INV_OPEN_STATE;
	inv->ref_count = 0;
	inv->game = NULL;

	sem_init(&inv->mutex, 0, 1);  //Initialize mutex to prevent simultaenous source and target access
	inv_ref(inv, "for creating the invitation");  //Set reference of invitation to 1
	/* Increment reference coutn of source and target clients */
	client_ref(source, "for association with an invitation");
	client_ref(target, "for association with an invitation");

	return inv;

}

INVITATION *inv_ref(INVITATION *inv, char *why) {
	sem_wait(&inv->mutex); //Protect ref_count with mutex
	inv->ref_count++;
	sem_post(&inv->mutex);
	return inv;
}

void inv_unref(INVITATION *inv, char *why) {
	sem_wait(&inv->mutex); //Protect ref_count with mutex
	inv->ref_count--;
	/* free invitation when ref count == 0 and decrease source and client ref_counts */
	if (inv->ref_count == 0) {
		client_unref(inv->source, "for deassociation with an invitation");
		client_unref(inv->target, "for deassociation with an invitation");
		free(inv);
	}
	sem_post(&inv->mutex);
}

/* Getter methods. Self explanatory */
CLIENT *inv_get_source(INVITATION *inv) {
	return inv->source;
}

CLIENT *inv_get_target(INVITATION *inv) {
	return inv->target;
}

GAME_ROLE inv_get_source_role(INVITATION *inv) {
	return inv->source_role;
}

GAME_ROLE inv_get_target_role(INVITATION *inv) {
	return inv->target_role;
}

GAME *inv_get_game(INVITATION *inv) {
	return inv->game;
}

int inv_accept(INVITATION *inv) {
	sem_wait(&inv->mutex);
	if (inv->state != INV_OPEN_STATE) {
		sem_post(&inv->mutex);
		return -1;
	}
	/* Create a new associated game */
	if((inv->game = game_create()) == NULL) {
		sem_post(&inv->mutex);
		return -1;
	}
	sem_post(&inv->mutex);
	return 0;
}

int inv_close(INVITATION *inv, GAME_ROLE role) {
	sem_wait(&inv->mutex); //Protect invitation from simultaneous access
	/* Do no close invitation when NULL role passed and game running */
	if (role == NULL_ROLE) {
		if (inv->state == INV_ACCEPTED_STATE || inv->state == INV_CLOSED_STATE) {  //Game running or already closed
			sem_post(&inv->mutex);
			return -1;
		 }
		else if (inv->state == INV_OPEN_STATE){  //If no game running
			inv->state = INV_CLOSED_STATE;
			sem_post(&inv->mutex);
			return 0;
		}
	}
	else {
		if (inv->state == INV_CLOSED_STATE) {  //Cannot close already closed invitation
			sem_post(&inv->mutex);
			return -1;
		}
		else if (inv->state == INV_ACCEPTED_STATE) {
			inv->state = INV_CLOSED_STATE;  //Close invitation
			if (!game_is_over(inv_get_game(inv))) {  //When the game is not over
				game_resign(inv_get_game(inv), role);
			}
			sem_post(&inv->mutex);
			return 0;
		}
		else if (inv->state == INV_OPEN_STATE) {//Either in OPEN or ACCEPTED state
			inv->state = INV_CLOSED_STATE;  //Close invitation
			sem_post(&inv->mutex);
			return 0;
		}
	}
	/* Never reach here */
	return -1;
}






