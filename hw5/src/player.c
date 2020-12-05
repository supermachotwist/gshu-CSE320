#include <stdlib.h>
#include <semaphore.h>
#include <math.h>
#include <string.h>

#include "player.h"

struct player {
	char name[1024];
	int rating;
	int ref_count;
	PLAYER *next;
	sem_t mutex;
};

PLAYER *player_create(char *name) {
	PLAYER *player = malloc(sizeof(PLAYER));
	strcpy(player->name,name);  //Save a copy of the username
	player->rating = 1500;
	player->ref_count = 0;
	player->next = NULL;  //Set next linked list player as NULL
	sem_init(&player->mutex, 0 , 1);

	player_ref(player, "on creation of a new player");
	return player;
}

PLAYER *player_ref(PLAYER *player, char *why) {
	sem_wait(&player->mutex);
	player->ref_count++;
	sem_post(&player->mutex);
	return player;
}

void player_unref(PLAYER *player, char *why) {
	sem_wait(&player->mutex);
	player->ref_count--;
	if (player->ref_count == 0) {
		free(player);
	}
	sem_post(&player->mutex);
}

char *player_get_name(PLAYER *player) {
	return player->name;
}

int player_get_rating(PLAYER *player) {
	return player->rating;
}

void player_post_result(PLAYER *player1, PLAYER *player2, int result) {
	double S1, S2, R1, R2, E1, E2;  //Score 1 and 2, Rating 1 and 2
	/* Set playet ratings */
	R1 = player_get_rating(player1);
	R2 = player_get_rating(player2);

	if (result == 0) {  //Draw
		S1 = 0.5;
		S2 = 0.5;
	}
	else if (result == 1) {  //Player 1 won
		S1 = 1;
		S2 = 0;
	}
	else if (result == 2) {  //Player 2 won
		S1 = 0;
		S2 = 1;
	}
	else {  //Invalid result
		return;
	}
	/* Update player ratings */
	E1 = 1/(1 + pow(10,((R2-R1)/400)));
	E2 = 1/(1 + pow(10,((R1-R2)/400)));

	sem_wait(&player1->mutex);
	player1->rating = R1 + (int)32*(S1-E1);
	sem_post(&player1->mutex);
	sem_wait(&player2->mutex);
 	player2->rating = R2 + (int)32*(S2-E2);
 	sem_post(&player2->mutex);
}
