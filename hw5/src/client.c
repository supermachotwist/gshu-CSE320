#include <stdlib.h>
#include <semaphore.h>
#include <string.h>

#include "client_registry.h"
#include "player_registry.h"
#include "client.h"
#include "player.h"
#include "jeux_globals.h"
#include "invitation.h"
#include "game.h"

/*=struct invitation {
	CLIENT *source;
	CLIENT *target;
	int id;
	int ref_count;
	int state;
	GAME_ROLE source_role;
	GAME_ROLE target_role;
	GAME *game;
	sem_t mutex;
	INVITATION *prev;
	INVITATION *next;  //Pointer to next invitation in linked list
}; */

/* Mapping between invitation and its id */
typedef struct mapping MAPPING;
struct mapping {
	uint8_t id;
	INVITATION *invitation;
};

/* Sentinel head of invitation list */
typedef struct inv_sentinel SENTINEL;

struct client {
	int ref_count;
	int fd;
	PLAYER *player;  //If player is NULL, client is logged out
	struct mapping *invitations[1000];  /* Sentinel head of invitation list */
	sem_t mutex;
};

/* Search client invitation list to find invitation associated with id */
INVITATION *client_inv_search(CLIENT *client, int id) {
	for (int i = 0; i < 1000; i++) {
		if (client->invitations[i] != NULL) {
			if (client->invitations[i]->id == id) {
				return client->invitations[i]->invitation;
			}
		}
	}
	return NULL;  //Failrue to find invitation
}

/* Search client invitation list to find id of associated invitation */
int client_id_search(CLIENT *client, INVITATION *inv) {
	sem_wait(&client->mutex);

	for (int i = 0; i < 1000; i++) {
		if (client->invitations[i] != NULL) {
			if (client->invitations[i]->invitation == inv) {
				sem_post(&client->mutex);
				return  client->invitations[i]->id;
			}
		}
	}
	/* Failure to find invitation */
	sem_post(&client->mutex);
	return -1;
}

CLIENT *client_create(CLIENT_REGISTRY *creg, int fd) {
	CLIENT *client = malloc(sizeof(CLIENT));
	client->ref_count = 0;
	client->fd = fd;
	client->player = NULL;  //Player is considered logged out when client's player is NULL
	sem_init(&client->mutex, 0, 1);
	for (int i = 0; i < 1000; i++) {
		client->invitations[i] = NULL;  //Set all invitations to NULL empty
	}

	client_ref(client, "for creation of new client");

	return client;
}

CLIENT *client_ref(CLIENT *client, char *why) {
	sem_wait(&client->mutex);
	client->ref_count++;
	sem_post(&client->mutex);
	return client;
}

void client_unref(CLIENT *client, char *why) {
	sem_wait(&client->mutex);
	client->ref_count--;

	/* free client upon 0 ref_count */
	if (client->ref_count == 0) {
		if (client->player != NULL) {
			player_unref(client->player, "for client loosing reference to player by being freed");
		}
		for (int i = 0; i < 1000; i++) {
			if (client->invitations[i] != NULL) {
				free(client->invitations[i]);
			}
		}
		free(client);
	}
	sem_post(&client->mutex);
}

int client_login(CLIENT *client, PLAYER *player) {
	sem_wait(&client->mutex);
	/* Check if client is already logged in */
	if (client->player != NULL) {
		return -1;
	}

	/* If client was not logged in */
	/* Check if name is already taken */
	if (creg_lookup(client_registry, player_get_name(player)) != NULL) {
		/* When name is already taken */
		return -1;
	}

	/* if name is not taken */
	client->player = player;
	player_ref(player, "for client logging in");

	sem_post(&client->mutex);
	return 0;

}

int client_logout(CLIENT *client) {
	sem_wait(&client->mutex);
	/* Check if client is already logged out */
	if (client->player == NULL) {
		return -1;
	}
	/* Revoke or decline invitation and remove from client list */
	for (int i = 0; i < 1000; i++) {
		/* Resign if a game is in progress */
		if (client->invitations[i] != NULL) {
			sem_post(&client->mutex);
			if (inv_get_game(client->invitations[i]->invitation) != NULL) {  //Check if invitation is accepted
				client_resign_game(client, client->invitations[i]->id);
			}
			/* Revoke invitation if current client is the source of the invitation */
			else if (inv_get_source(client->invitations[i]->invitation) == client) {
				if(client_revoke_invitation(client, client->invitations[i]->id) == -1) {
					return -1;
				}
			}
			/* Decline invitation if current client is not the source of the invitation */
			else {
				if (client_decline_invitation(client, client->invitations[i]->id) == -1) {
					return -1;
				}
			}
			sem_wait(&client->mutex);
		}
	}
	player_unref(client->player, "for client logging out");
	client->player = NULL;
	sem_post(&client->mutex);
	return 0; //Successfully logged out
}

PLAYER *client_get_player(CLIENT *client) {
	return client->player;
}

int client_get_fd(CLIENT *client) {
	return client->fd;
}

int client_send_packet(CLIENT *player, JEUX_PACKET_HEADER *pkt, void *data) {
	/* Gain exclusive access to file descriptor */
	sem_wait(&player->mutex);
	if (proto_send_packet(client_get_fd(player), pkt, data)) {  //If sending fails
		sem_post(&player->mutex);
		return -1;
	}
	else {
		sem_post(&player->mutex);
		return 0;
	}
}

int client_send_ack(CLIENT *client, void *data, size_t datalen) {
	JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
	hdr->type = JEUX_ACK_PKT;
	hdr->size = htons(datalen);
	hdr->id = 0;
	hdr->role = 0;

	if (client_send_packet(client, hdr, data) == -1) {  //Check for error
		free(hdr);
		return -1;
	}
	free(hdr);
	return 0;
}

int client_send_nack(CLIENT *client) {
	JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
	hdr->type = JEUX_NACK_PKT;
	hdr->size = 0;
	hdr->id = 0;
	hdr->role = 0;

	if (client_send_packet(client, hdr, NULL) == -1) {  //Check for error
		free(hdr);
		return -1;
	}
	free(hdr);
	return 0;
}

int client_add_invitation(CLIENT *client, INVITATION *inv) {
	sem_wait(&client->mutex);
	MAPPING *currentmap;
	for (int i = 0; i < 1000; i++) {
		if(client->invitations[i] == NULL) {
			/* Empty linked list spot is found */
			currentmap = malloc(sizeof(MAPPING));
			currentmap->id = i;
			currentmap->invitation = inv;
			client->invitations[i] = currentmap;
			break;
		}
	}
	inv_ref(inv,"for adding to client list of inviations");
	sem_post(&client->mutex);
	return currentmap->id;
}

int client_remove_invitation(CLIENT *client, INVITATION *inv) {
	sem_wait(&client->mutex);

	for (int i = 0; i < 1000; i++) {
		if (client->invitations[i]->invitation == inv) {
			/* If invitation was found remove*/
			int retid;  //Id of removed invitation
			retid = client->invitations[i]->id;
			free(client->invitations[i]);
			client->invitations[i] = NULL;
			inv_unref(inv,"for removing to client list of inviations");
			sem_post(&client->mutex);
			return retid;
		}
	}
	/* Failure to find invitation */
	sem_post(&client->mutex);
	return -1;
}

int client_make_invitation(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role) {
	INVITATION *inv;
	uint8_t hdrid; //Id of for packet header
	/* Create invitation */
	if ((inv = inv_create(source, target, source_role, target_role)) == NULL) {  //Check for errors
		return -1;
	}

	/* Add invitation to source and client invitation lists */
	hdrid = client_add_invitation(source, inv);
	if (hdrid == -1) {  //Check for errors
		return -1;
	}
	inv_ref(inv, "for being associated with a source client");

	if (client_add_invitation(target, inv) == -1) {  //Check for errors
		return -1;
	}
	inv_ref(inv, "for being associated with a target client");

	/* Send invited packet to target */
	char *name = player_get_name(client_get_player(source));
	JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
	hdr->type = JEUX_INVITED_PKT;
	hdr->size = htons(strlen(name) + 1);
	hdr->role = target_role;
	hdr->id = hdrid;
	if (client_send_packet(target, hdr, name)) {
		free(hdr);
		return -1;
	}
	free(hdr);
	return hdrid;
}

int client_revoke_invitation(CLIENT *client, int id) {
	INVITATION *inv;
	/* Search client for invitation */
	if ((inv = client_inv_search(client, id)) == NULL) {
		return -1;
	}
	/* Check if source of invitation is revoking client */
	if (inv_get_source(inv) != client) {
		return -1;
	}
	/* Check if invitation is in the open state = No game in progress */
	if (inv_get_game(inv) != NULL) {
		return -1;
	}

	/* Check game role of client */
	GAME_ROLE role;
	CLIENT *source = inv_get_source(inv);
	CLIENT *target = inv_get_target(inv);
	/* If the client is the source of the invitation */
	if (source == client) {
		role = inv_get_source_role(inv);
	}
	/* If the client is the target of the invitation */
	else if(target == client) {
		role = inv_get_target_role(inv);
	}
	else {
		return -1;
	}
	/* Close invitation */
	if (inv_close(inv, role)) {
		return -1;
	}

	int targetid;
	/* Remove invitation from source */
	if (client_remove_invitation(client, inv) == -1) {
		return -1;
	}
	/* Remove invitation from target */
	if ((targetid = client_remove_invitation(target, inv)) == -1) {
		return -1;
	}

	/* Invitation is successfully revoked */
	/* Send REVOKED packt to target */
	JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
	hdr->type = JEUX_REVOKED_PKT;
	hdr->id = targetid;
	hdr->size = 0;
	if (client_send_packet(target, hdr, NULL)) {
		free(hdr);
		return -1;
	}
	free(hdr);
	return 0;  //Success
}

int client_decline_invitation(CLIENT *client, int id) {
	INVITATION *inv;
	/* Search client for invitation */
	if ((inv = client_inv_search(client, id)) == NULL) {
		return -1;
	}
	/* Check if target of invitation is declining client */
	if (inv_get_target(inv) != client) {
		return -1;
	}

	/* Check game role of client */
	GAME_ROLE role;
	CLIENT *source = inv_get_source(inv);
	CLIENT *target = inv_get_target(inv);
	/* If the client is the source of the invitation */
	if (source == client) {
		role = inv_get_source_role(inv);
	}
	/* If the client is the target of the invitation */
	else if(target == client) {
		role = inv_get_target_role(inv);
	}
	else {
		return -1;
	}
	/* Close invitation */
	if (inv_close(inv, role)) {
		return -1;
	}

	int sourceid;
	/* Remove invitation from target */
	if (client_remove_invitation(client, inv) == -1) {
		return -1;
	}
	/* Remove invitation from source */
	if ((sourceid = client_remove_invitation(source, inv)) == -1) {
		return -1;
	}

	/* Invitation is successfully revoked */
	/* Send REVOKED packt to target */
	JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
	hdr->type = JEUX_DECLINED_PKT;
	hdr->id = sourceid;
	hdr->size = 0;
	if (client_send_packet(source, hdr, NULL)) {
		free(hdr);
		return -1;
	}
	free(hdr);
	return 0;  //Success
}

int client_accept_invitation(CLIENT *client, int id, char **strp) {
	INVITATION *inv;
	/* Search client for invitation */
	if ((inv = client_inv_search(client, id)) == NULL) {
		return -1;
	}
	/* Check if target of invitation is revoking client */
	if (inv_get_target(inv) != client) {
		return -1;
	}
	/* Successfully accept the invitation */
	/* Create a new game and associate it with the invitation*/
	if (inv_accept(inv)) {
		return -1;
	}

	/* Send ACCEPTED packt to source */
	CLIENT *source = inv_get_source(inv);
	int sourceid = client_id_search(source, inv);
	JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
	hdr->type = JEUX_ACCEPTED_PKT;
	hdr->id = sourceid;

	/* Set strp */
	if (inv_get_target_role(inv) == FIRST_PLAYER_ROLE) {
		*strp = game_unparse_state(inv_get_game(inv));
	}
	else if(inv_get_target_role(inv) == SECOND_PLAYER_ROLE) {
		*strp = NULL;
	}

	/* Check who moves first */
	if (*strp != NULL) {  //the accepting client is not the first player to move
		hdr->size = 0;
		if (client_send_packet(source, hdr, NULL)) {
			free(hdr);
			return -1;
		}
	}
	else  {  //the accepting client is the first player to move
		hdr->size = htons(strlen(game_unparse_state(inv_get_game(inv)) + 1));
		if (client_send_packet(source, hdr, game_unparse_state(inv_get_game(inv)))) {
			free(hdr);
			return -1;
		}
	}
	free(hdr);
	return 0;  //Success
}

int client_resign_game(CLIENT *client, int id) {
	INVITATION *inv;
	/* Search client for invitation */
	if ((inv = client_inv_search(client, id)) == NULL) {
		return -1;
	}
	/* Check if invitation is in the accepted state = No game in progress */
	if (inv_get_game(inv) == NULL || game_is_over(inv_get_game(inv))) {
		return -1;
	}
	/* Successfully resign the invitation */
	/* Check game role of client */
	GAME_ROLE role;
	CLIENT *source = inv_get_source(inv);
	CLIENT *target = inv_get_target(inv);
	/* If the client is the source of the invitation */
	if (source == client) {
		role = inv_get_source_role(inv);
	}
	/* If the client is the target of the invitation */
	else if(target == client) {
		role = inv_get_target_role(inv);
	}
	else {
		return -1;
	}
	/* Close invitation */
	if (inv_close(inv, role)) {
		return -1;
	}

	/* Remove invitation from target */
	if (client_remove_invitation(target, inv) == -1) {
		return -1;
	}
	/* Remove invitation from source */
	if ((client_remove_invitation(source, inv)) == -1) {
		return -1;
	}

	if (source == client) {
		/* Send RESIGNED packt to target */;
		int targetid = client_id_search(target, inv);  //Find id of invitation of target
		JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
		hdr->type = JEUX_RESIGNED_PKT;
		hdr->id = targetid;
		hdr->size = 0;
		if (client_send_packet(target, hdr, NULL)) {
			free(hdr);
			return -1;
		}
		free(hdr);
		return 0;
	}

	if (target == client) {
		/* Send RESIGNED packt to source */;
		int sourceid = client_id_search(source, inv);  //Find id of invitation of client
		JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
		hdr->type = JEUX_RESIGNED_PKT;
		hdr->id = sourceid;
		hdr->size = 0;
		if (client_send_packet(source, hdr, NULL)) {
			free(hdr);
			return -1;
		}
		free(hdr);
		return 0;
	}
	return -1;
}

int client_make_move(CLIENT *client, int id, char *move) {
	INVITATION *inv;
	/* Search client for invitation */
	if ((inv = client_inv_search(client, id)) == NULL) {
		return -1;
	}

	/* Check game role of client */
	GAME_ROLE role;
	CLIENT *source = inv_get_source(inv);
	CLIENT *target = inv_get_target(inv);
	/* If the client is the source of the invitation */
	if (source == client) {
		role = inv_get_source_role(inv);
	}
	/* If the client is the target of the invitation */
	else if(target == client) {
		role = inv_get_target_role(inv);
	}
	else {
		return -1;
	}

	/* Parse the move */
	GAME *game = inv_get_game(inv);
	GAME_MOVE *game_move;
	if ((game_move = game_parse_move(game, role, move)) == NULL) {
		return -1;
	}

	/* Apply move */
	if (game_apply_move(game, game_move)) {
		return -1; //Game move failed
	}

	/* Send MOVED packet to opponent */
	if (source == client) {  //Opponent is the target
		JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
		hdr->type = JEUX_MOVED_PKT;
		hdr->id = client_id_search(target, inv);
		hdr->size = htons(strlen(game_unparse_state(game))) + 1;
		if (client_send_packet(target, hdr, game_unparse_state(game))) {  //If packet sending fails
			free(hdr);
			return -1;
		}
	}

	if (target == client) {  //Opponent is the source
		JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
		hdr->type = JEUX_MOVED_PKT;
		hdr->id = client_id_search(source, inv);
		hdr->size = htons(strlen(game_unparse_state(game))) + 1;
		if (client_send_packet(source, hdr, game_unparse_state(game))) {  //If packet sending fails
			free(hdr);
			return -1;
		}
	}

	/* Check if game is terminated */
	if (game_is_over(game) == 1) {  //If game is terminated
		/* Send ENDED packet */
		int sourceid = client_id_search(source, inv);
		int targetid = client_id_search(target, inv);
		GAME_ROLE winner = game_get_winner(game);
		/* Send to source */
		JEUX_PACKET_HEADER *hdr = malloc(sizeof(JEUX_PACKET_HEADER));
		hdr->type = JEUX_ENDED_PKT;
		hdr->id = sourceid;
		hdr->role = winner;
		hdr->size = 0;
		if (client_send_packet(source, hdr, NULL)) {
			free(hdr);
			return -1;
		}

		/* Send to target */
		hdr->id = targetid;
		if (client_send_packet(target, hdr, NULL)) {
			free(hdr);
			return -1;
		}
		free(hdr);

		/* Remove invitation from source */
		if (client_remove_invitation(source, inv) == -1) {
			return -1;
		}
		/* Remove invitation from target */
		if ((targetid = client_remove_invitation(target, inv)) == -1) {
			return -1;
		}

		/* Find player roles */
		PLAYER *player1;
		PLAYER *player2;
		if (inv_get_source_role(inv) == FIRST_PLAYER_ROLE) {
			player1 = client_get_player(source);
			player2 = client_get_player(target);
		}
		else if (inv_get_source_role(inv) == SECOND_PLAYER_ROLE) {
			player1 = client_get_player(target);
			player2 = client_get_player(source);
		}

		/* Update both players ratings */
		player_post_result(player1, player2, winner);
	}
	return 0; //Success
}