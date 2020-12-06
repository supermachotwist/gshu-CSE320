#include <stdlib.h>
#include <math.h>
#include <semaphore.h>
#include <string.h>

#include "game.h"



struct game {
	int ref_count;
	int isOver;  //Tracks whether the game is in progress or finished
	GAME_ROLE winner;  //Track the winner of the game. NULL_ROLE if game not over or draw
	GAME_ROLE turn;  //Track the current game role of player whose turn it is
	char state[3][3];  //Tracks the current state of the game
	sem_t mutex;
};

struct game_move {
	char space;  //Char between [1-9] representing the space the move is to be made
	GAME_ROLE player;  //Player X or O to place the move. X = Player 1. O = Player 2
};

GAME *game_create(void) {
	GAME *game = malloc(sizeof(game));
	game->ref_count = 0;
	game->isOver = 0;
	game->winner = NULL_ROLE;
	game->turn = FIRST_PLAYER_ROLE;
	/* Empty game state */
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			game->state[i][j] = ' ';
		}
	}
	sem_init(&game->mutex,0 , 1);
	game_ref(game, "for creation of new game");
	return game;
}

GAME *game_ref(GAME *game, char *why) {
	sem_wait(&game->mutex);
	game->ref_count++;
	sem_post(&game->mutex);
	return game;
}

void game_unref(GAME *game, char *why) {
	sem_wait(&game->mutex);
	game->ref_count--;
	if(game->ref_count == 0) {
		free(game);
	}
	sem_post(&game->mutex);
}

int game_apply_move(GAME *game, GAME_MOVE *move) {
	int row = floor((move->space - 1)/3.0); //Convert move into row
	int column = (move->space - 1)%3;	//Convert move into column

	sem_wait(&game->mutex);
	/* Error if space is taken */
	if (game->state[row][column] != ' ') {
		sem_post(&game->mutex);
		return -1;
	}
	if (game->turn != move->player) {
		sem_post(&game->mutex);
		return -1;
	}
	/* When space is free */
	/* Change turn */
	if (move->player == FIRST_PLAYER_ROLE) {
		game->turn = SECOND_PLAYER_ROLE;
		game->state[row][column] = 'X';  //Set piece on board
	}
	else if (move->player == SECOND_PLAYER_ROLE) {
		game->turn = FIRST_PLAYER_ROLE;
		game->state[row][column] = 'O';  //Set piece on board
	}

	char win;  //Checking for winner
	/*Check if game is complete */
	/* Check row for winner */
	for (int i = 0; i < 3; i++) {
		if (game->state[row][i] == ' ') {
			break;
		}
		if (i == 0) {
			win = game->state[row][i];
			continue;
		}
		if (game->state[row][i] == win) {  //If piece is equal to the previous piece
			if (i == 2) {  //Player won. Game is over.
				if (win == 'X') {
					game->winner = FIRST_PLAYER_ROLE;
				}
				else if (win == 'O') {
					game->winner = SECOND_PLAYER_ROLE;
				}
				game->isOver = 1;
				game->turn = NULL_ROLE;
				sem_post(&game->mutex);
				return 0;
			}
			continue;
		}
		else {
			break;
		}
	}

	/* Check column for winner*/
	for (int i = 0; i < 3; i++) {
		if (game->state[i][column] == ' ') {
			break;
		}
		if (i == 0) {
			win = game->state[i][column];
			continue;
		}
		if (game->state[i][column] == win) {  //If piece is equal to the previous piece
			if (i == 2) {  //Player won. Game is over.
				if (win == 'X') {
					game->winner = FIRST_PLAYER_ROLE;
				}
				else if (win == 'O') {
					game->winner = SECOND_PLAYER_ROLE;
				}
				game->isOver = 1;
				game->turn = NULL_ROLE;
				sem_post(&game->mutex);
				return 0;
			}
			continue;
		}
		else {
			break;
		}
	}

	/* Check diagonals for winner */
	for (int i = 0; i < 3; i++) {
		if (game->state[i][i] == ' ') {
			break;
		}
		if (i == 0) {
			win = game->state[i][i];
			continue;
		}
		if (game->state[i][i] == win) {  //If piece is equal to the previous piece
			if (i == 2) {  //Player won. Game is over.
				if (win == 'X') {
					game->winner = FIRST_PLAYER_ROLE;
				}
				else if (win == 'O') {
					game->winner = SECOND_PLAYER_ROLE;
				}
				game->isOver = 1;
				game->turn = NULL_ROLE;
				sem_post(&game->mutex);
				return 0;
			}
			continue;
		}
		else {  //If not
			break;
		}
	}

	/* Check diagonal for winner */
	for (int i = 0; i < 3; i++) {
		if (game->state[2-i][i] == ' ') {
			break;
		}
		if (i == 0) {
			win = game->state[2-i][i];
			continue;
		}
		if (game->state[2-i][i] == win) {  //If piece is equal to the previous piece
			if (i == 2) {  //Player won. Game is over.
				if (win == 'X') {
					game->winner = FIRST_PLAYER_ROLE;
				}
				else if (win == 'O') {
					game->winner = SECOND_PLAYER_ROLE;
				}
				game->isOver = 1;
				game->turn = NULL_ROLE;
				sem_post(&game->mutex);
				return 0;
			}
			continue;
		}
		else {
			break;
		}
	}

	/*Check for draw */
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (game->state[i][j] == ' ') {
				sem_post(&game->mutex);
				return 0;  //Return without draw
			}
		}
	}
	/* Game is a draw */
	game->winner = NULL_ROLE;
	game->isOver = 1;
	game->turn = NULL_ROLE;
	sem_post(&game->mutex);
	return 0;

}

int game_resign(GAME *game, GAME_ROLE role) {
	sem_wait(&game->mutex);
	if (game->isOver == 1) {  //Game is already terminated
		sem_post(&game->mutex);
		return -1;
	}
	game->isOver = 1;
	game->turn = NULL_ROLE;
	if (role == FIRST_PLAYER_ROLE) {
		game->winner = SECOND_PLAYER_ROLE;
	}
	else if (role == SECOND_PLAYER_ROLE) {
		game->winner = FIRST_PLAYER_ROLE;
	}

	sem_post(&game->mutex);
	return 0;
}

char *game_unparse_state(GAME *game) {
	sem_wait(&game->mutex);
	char *gamestr = malloc(30);  //Holds state of game
	*gamestr = '\0';  //Initially set to empty string
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			strncat(gamestr, &game->state[i][j], 1);  //Concat X,O or whitespace
			if (j != 2) {
				strcat(gamestr, "|");  //Concat vertical line
			}
		}
		if (i != 2) {
			strcat(gamestr, "\n-----\n");  //Concat horizontal line
		}
		else {
			strcat(gamestr, "\n");
		}
	}
	sem_post(&game->mutex);
	return gamestr;
}

int game_is_over(GAME *game) {
	return game->isOver;
}

GAME_ROLE game_get_winner(GAME *game) {
	return game->winner;
}

GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str) {
	sem_wait(&game->mutex);
	GAME_MOVE *game_move = malloc(sizeof(GAME_MOVE));
	if (strlen(str) != 1) {
		sem_post(&game->mutex);
		free(game_move);
		return NULL;  //Unproper String error
	}
	if ((game_move->space = atoi(str)) == 0) {  //TODO: Switch atoi with strtol
		sem_post(&game->mutex);
		free(game_move);
		return NULL;  //Unproper String error
	}
	if (game_move->space < 1 || game_move->space > 9) {  //Check if string is proper int [1-9]
		sem_post(&game->mutex);
		free(game_move);
		return NULL;  //Unproper String error
		}
	game_move->player = role;
	sem_post(&game->mutex);
	return game_move;
}

char *game_unparse_move(GAME_MOVE *move) {
	char *str = malloc(2);
	strncat(str, &move->space, 1);  //String is in in the form of a single char digit ['1'-'9']
	return 0;
}