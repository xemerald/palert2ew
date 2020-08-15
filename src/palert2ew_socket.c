/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* Network related header include */
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

/* Earthworm environment header include */
#include <earthworm.h>

#define LISTENQ            128
#define RECONNECT_INTERVAL 15000


/*
 *  ConstructSocket() - Construct Palert or Client connection
 *                        socket.
 */
int ConstructSocket( const char *ip, const char *port )
{
	int sd;
	int sock_opt = 1;

	struct addrinfo  hints;
	struct addrinfo *servinfo, *p;
	struct timeval   timeout;          /* Socket connect timeout */


/* Initialize the address info structure */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

/* Set to passive connect if we are trying to construct listening socket */
	if ( ip == (char *)NULL ) hints.ai_flags = AI_PASSIVE;

/* Get IP & port information */
	if ( getaddrinfo(ip, port, &hints, &servinfo) ) {
		logit("e", "palert2ew: Get connection address info error!\n");
		return -1;
	}

/* Setup socket */
	for ( p=servinfo; p!=NULL; p=p->ai_next) {
		if ( (sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1 ) {
			logit("e", "palert2ew: Construct Palert connection socket error!\n");
			return -1;
		}

	/* Set connection timeout to 15 seconds */
		timeout.tv_sec  = 15;
		timeout.tv_usec = 0;

	/* Setup characteristics of socket */
		setsockopt(sd, IPPROTO_TCP, TCP_QUICKACK, &sock_opt, sizeof(sock_opt));
		setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt));
		setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	/* Bind socket to listening port */
		if ( ip == (char *)NULL ) {
			if ( bind(sd, p->ai_addr, p->ai_addrlen) == -1 ) {
				logit("e", "palert2ew: Bind Palert listening socket error!\n");
				return -1;
			}
		}

		break;
	}

	if ( p == NULL ) {
		logit("e", "palert2ew: Construct Palert connection socket failed!\n");
		return -1;
	}

/* Listen on socket if we are using independent server mode */
	if ( ip == (char *)NULL ) {
		if ( listen(sd, LISTENQ) ) {
			logit("e", "palert2ew: Listen Palert connection socket error!\n");
			return -1;
		}
		else {
			logit("o", "palert2ew: Listen Palert connection socket ready!\n");
		}
	}
/* Connect to the Palert server if we are using dependent client mode */
	else {
		if ( connect(sd, servinfo->ai_addr, servinfo->ai_addrlen) < 0 ) {
			logit("e", "palert2ew: Connect to Palert server error!\n");
			return -1;
		}
		else {
			logit("o", "palert2ew: Connection to Palert server %s success!\n", ip);
		}
	}

	freeaddrinfo(servinfo);
/* End construct Palert connection socket */

	return sd;
}

/*******************************************************************
*  ReConstructSocket( ) -- Reconstruct Palert or Client connection *
*                          socket.                                 *
*  Arguments:                                                      *
*    ip   = String of IP address of server.                        *
*    port = String of connecting or listening port.                *
*    sd   = Pointer to the socket.                                 *
*  Returns:                                                        *
*    >0 = Normal, refer to the socket number.                      *
*    -1 = Error, reconstruct socket error.                         *
********************************************************************/
int ReConstructSocket( const char *ip, const char *port, int *sd )
{
	uint32_t count = 0;


	if ( *sd != -1 ) close(*sd);

/* Do until we success getting socket or exceed 100 times */
	while ( (*sd = ConstructSocket(ip, port)) == -1 ) {
	/* Try 100 times */
		if ( ++count > 100 ) {
			logit( "e", "palert2ew: Reconstruct socket failed; exiting!\n" );
			return -1;
		}
	/* Waiting for a while */
		sleep_ew(RECONNECT_INTERVAL);
	}

	logit("t", "palert2ew: Reconstruct socket success!\n");

	return *sd;
}
