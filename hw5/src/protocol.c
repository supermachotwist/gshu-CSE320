#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>

#include "protocol.h"

int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
	ssize_t i;

	/* Set timestamp of header */
	struct timespec *tp = malloc(sizeof(struct timespec));
	if (clock_gettime(CLOCK_REALTIME, tp) == -1) {
		return -1;
	}
	hdr->timestamp_sec = tp->tv_sec;
	hdr->timestamp_nsec = tp->tv_nsec;

	i = write(fd, hdr, sizeof(JEUX_PACKET_HEADER));
	if (i == -1)
		/* Failure, errno is set by write */
		return -1;
	/* Check for EOF */
	else if (i == 0) {
		return -1;
	}
	/* Check for shortcount */
	else if(i < sizeof(JEUX_PACKET_HEADER))
		write(fd, hdr, sizeof(JEUX_PACKET_HEADER) - i);

	if (ntohs(hdr->size) != 0) {
		i = write(fd, data, ntohs(hdr->size));
		if (i == -1)
			/* Failure, errno is set by write */
			return -1;
		/* Check for EOF */
		else if (i == 0) {
			return -1;
		}
		/* Check for shortcount and EOF */
		else if(i < ntohs(hdr->size))
			write(fd, data, ntohs(hdr->size) - i);
	}
	/* Success */
	return 0;
}

int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
	ssize_t i;
	*payloadp = NULL;

	i = read(fd, hdr, sizeof(JEUX_PACKET_HEADER));
	if (i == -1)
		/* Failure, errno is set by read */
		return -1;
	/* Check for EOF */
	else if (i == 0) {
		return -1;
	}
	/* Check for shortcount */
	else if(i < sizeof(JEUX_PACKET_HEADER))
		read(fd, hdr, sizeof(JEUX_PACKET_HEADER) - i);

	if (ntohs(hdr->size) != 0) {
		*payloadp = malloc(ntohs(hdr->size));
		i = read(fd, *payloadp, ntohs(hdr->size));
		if (i == -1)
			/* Failure, errno is set by read */
			return -1;
		/* Check for EOF */
		else if (i == 0) {
			return -1;
		}
		/* Check for shortcount */
		else if(i < ntohs(hdr->size))
			read(fd, *payloadp, ntohs(hdr->size) - i);
	}
	/* Sucess */
	return 0;
}
