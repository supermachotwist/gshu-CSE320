#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <signal.h>

#include "protocol.h"
#include "player.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"

/* Client Registry Definition */
struct client_registry {
	int size;  //Number of currently registered clients
	sem_t empty; //semaphore to check wheter empty or not
	sem_t mutex; //Mutex to protect client registry
	CLIENT *clients[MAX_CLIENTS];  //Array of client pointers
};

void *jeux_client_service(void *arg) {

	int fd = *(int *)arg;  //Store file descriptor

	free (arg);
	pthread_detach(pthread_self());  //Detach thread

	JEUX_PACKET_HEADER recvhdr;
	void *payload;

	PLAYER *player;
	CLIENT *srcclient;

	srcclient = creg_register(client_registry, fd);

	int loginflag = 0;  //Flag to check wheter client is logged in
	int flag = 0;  //Check if recv_packet fails

	while (1) {
		/*  If sigpipe was met, unregister client and exit thread
			Resign any ongoing games
			Revoke and decline any outgoing invitation */
		if (flag == 1) {
			flag = 0;
			client_logout(srcclient);
			player_unref(player, "because server thread is discarding reference to logged in player");
			creg_unregister(client_registry, srcclient);
			pthread_exit(NULL);
		}
		/* Wait for arrival of packet */
		int err = proto_recv_packet(fd, &recvhdr, &payload);
		if (err) {  //If recieving failed, skip
			flag = 1;
			if (payload != NULL) {
				free(payload);
			}
			continue;
		}

		/* Check if client is logged in */
		if (loginflag == 0) {
			if (recvhdr.type == JEUX_LOGIN_PKT){
				payload = realloc(payload, ntohs(recvhdr.size) + 1);
				((char *)payload)[ntohs(recvhdr.size)] = '\0';  //NULL terminate the payload

				char *user = payload;
				player = preg_register(player_registry, user); //Create the player to login
				if (player == NULL) {
					client_send_nack(srcclient);
				}

				if(client_login(srcclient, player)) { //If error send nack
					client_send_nack(srcclient);
				}
				else {
					client_send_ack(srcclient, NULL, 0); //On successful login
					loginflag = 1;
				}
			}
			else {
				client_send_nack(srcclient);
			}

		}
		else if (loginflag == 1) {
			if (recvhdr.type == JEUX_LOGIN_PKT) { //When client tries to login twice
				client_send_nack(srcclient);
			}

			/* Users packet */
			else if (recvhdr.type == JEUX_USERS_PKT) {
				PLAYER **players = creg_all_players(client_registry);  //Get list of players
				char *data = (char *)malloc(1);  //Initialize data
				strcpy(data, "\0");  //intialize empty string
				int i = 0;  //Index
				char *name;
				int rating;
				char rstring[10];
				while (*(players + i) != NULL) {
					name = player_get_name(*(players + i));
					rating = player_get_rating(*(players + i));
					sprintf(rstring, "%d", rating);  //convert int to string
					/* Allocate space for string, tab and newline */
					size_t datalength = strlen(data);
					data = (char *)realloc((void *)data, datalength + strlen(name) + strlen(rstring) + 3);
					strcat(data,name);
					strcat(data,"\t");
					strcat(data,rstring);
					strcat(data, "\n");
					player_unref(*(players + i), "Remove from Users packet list");

					i++;
				}
				JEUX_PACKET_HEADER hdr;
				hdr.type = JEUX_ACK_PKT;
				hdr.size = ntohs(strlen(data) + 1);
				client_send_packet(srcclient, &hdr, data);
				free(data);
				free(players);
			}

			/* Invite packet */
			else if (recvhdr.type == JEUX_INVITE_PKT) {
				/* TODO: NULL ternimate payload */
				payload = realloc(payload, ntohs(recvhdr.size) + 1);
				((char *)payload)[ntohs(recvhdr.size)] = '\0';  //NULL terminate the payload
				char *user = payload;
				CLIENT *destclient = creg_lookup(client_registry,user);
				if (destclient == NULL || srcclient == destclient) {  //User not found
					client_send_nack(srcclient);  //Send nack to source
					if (payload != NULL) {
						free(payload);
					}
					continue;
				}
				/* Set gameroles of source */
				GAME_ROLE srcrole;
				if (recvhdr.role == 1)
					srcrole = 2;
				else if (recvhdr.role == 2)
					srcrole = 1;

				int id;  //Id of invitation
				if ((id = client_make_invitation(srcclient, destclient, srcrole, recvhdr.role)) == -1) { //if failed to send invited packet
					client_send_nack(srcclient);
					if (payload != NULL) {
						free(payload);
					}
					continue;
				}

				/* Send ack to source */
				JEUX_PACKET_HEADER srchdr;
				srchdr.type = JEUX_ACK_PKT;
				srchdr.id = id; //Set id
				if (client_send_packet(srcclient, &srchdr, NULL)) {
					client_send_nack(srcclient);
					if (payload != NULL) {
						free(payload);
					}
					continue;
				}
			}

			/* Revoke packet */
			else if (recvhdr.type == JEUX_REVOKE_PKT) {
				/* Try to revoke invitation */
				if (client_revoke_invitation(srcclient, recvhdr.id)) {  //if failed to revoke invitation
					client_send_nack(srcclient);
					if (payload != NULL) {
						free(payload);
					}
					continue;
				}

				/* If revoking is successful */
				/* Send ack to source */
				client_send_ack(srcclient, NULL, 0);
			}

			/* Decline packet */
			else if (recvhdr.type == JEUX_DECLINE_PKT) {
				/* Try to decline the invitation */
				if (client_decline_invitation(srcclient, recvhdr.id)) {  //if failed to decline packet
					client_send_nack(srcclient);
					if (payload != NULL) {
						free(payload);
					}
					continue;
				}

				/* If decline is successful */
				/* Send ack to target */
				client_send_ack(srcclient, NULL, 0);
			}

			/* Accept packet */
			else if (recvhdr.type == JEUX_ACCEPT_PKT) {
				char *strp; // to hold initial game state
				/* Try to accept the invitation */
				if (client_accept_invitation(srcclient, recvhdr.id, &strp)) {
					client_send_nack(srcclient);
					if (payload != NULL) {
						free(payload);
					}
					continue;
				}

				/* If accept is successful */
				/* Check targets gamerole */
				if (strp == NULL) { //If accepting client is not the first player to move
					client_send_ack(srcclient, NULL, 0);
				}
				else {  //If accepting client is first player to move
					JEUX_PACKET_HEADER hdr;
					hdr.type = JEUX_ACK_PKT;
					hdr.size = ntohs(strlen(strp) + 1);
					client_send_packet(srcclient, &hdr, strp);
				}
				free(strp);
			}

			/* Move packet */
			else if (recvhdr.type == JEUX_MOVE_PKT) {
				char *move = payload;
				move[1] = '\0';  //Truncate string to first character
				if (client_make_move(srcclient, recvhdr.id, move)) {  //If failure
					client_send_nack(srcclient);
				}
				else {
					/* If move was successful */
					client_send_ack(srcclient, NULL, 0);
				}
			}

			else if(recvhdr.type == JEUX_RESIGN_PKT)  {
				if (client_resign_game(srcclient, recvhdr.id)) {
					client_send_nack(srcclient);
				}
				else {
					client_send_ack(srcclient, NULL, 0);
				}

			}
		}
		if (payload != NULL) {
			free(payload);
		}
	}
}