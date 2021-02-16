/*
 *
 */
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
/* Network related header include */
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <palert2ew.h>
#include <palert2ew_list.h>
#include <palert2ew_msg_queue.h>

#define FW_PCK_HEADER_LENGTH    4
#define RECONNECT_TIMES         10
#define RECONNECT_INTERVAL_MSEC 15000

/* */
typedef struct {
	uint16_t serial;
	uint16_t length;
} FW_PCK_HEADER;

/* */
static int reconstruct_connect_sock( void );
static int construct_connect_sock( const char *, const char * );

/* */
static volatile int  ClientSocket = -1;
static const char   *_ServerIP    = NULL;
static const char   *_ServerPort  = NULL;

/*
 * pa2ew_client_init() - Initialize the dependent Palert client.
 */
int pa2ew_client_init( const char *ip, const char *port )
{
/* Setup constants */
	_ServerIP   = ip;
	_ServerPort = port;
/* Construct the accept socket */
	ClientSocket = construct_connect_sock( ip, port );

	return ClientSocket;
}

/*
 * pa2ew_client_end() - End process of Palert client.
 */
void pa2ew_client_end( void )
{
	close(ClientSocket);

	return;
}

/*
 * pa2ew_client_stream() - Receive the messages from the socket of forward server
 *                         and send it to the MessageStacker.
 */
int pa2ew_client_stream( void )
{
	static uint8_t buffer[PA2EW_RECV_BUFFER_LENGTH] = { 0 };
	static uint8_t sync_errors = 0;

	int            ret       = 0;
	int            data_read = 0;
	int            data_req  = FW_PCK_HEADER_LENGTH;
	FW_PCK_HEADER *header    = (FW_PCK_HEADER *)buffer;
	_STAINFO      *staptr    = NULL;

	memset(buffer, 0, PA2EW_RECV_BUFFER_LENGTH);
	do {
		if ( (ret = recv(ClientSocket, buffer + data_read, data_req, 0)) <= 0 ) {
			if ( errno == EINTR ) {
				sleep_ew(100);
				continue;
			}
			else if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT ) {
				logit("e", "palert2ew: Connection to Palert server is timeout, reconnect...\n");
			}
			else if ( ret == 0 ) {
				logit("e", "palert2ew: Connection to Palert server is closed, reconnect...\n");
			}
			else {
				logit("e", "palert2ew: Fatal error on Palert server, exiting!\n");
				return PA2EW_RECV_FATAL_ERROR;
			}
		/* Wait for additional time, then do the reconnection */
			sleep_ew(RECONNECT_INTERVAL_MSEC);
			if ( reconstruct_connect_sock() < 0 )
				return PA2EW_RECV_CONNECT_ERROR;

			data_read = 0;
			data_req  = FW_PCK_HEADER_LENGTH;
			continue;
		}
	/* */
		if ( (data_read += ret) >= FW_PCK_HEADER_LENGTH )
			data_req = header->length + FW_PCK_HEADER_LENGTH - data_read;
	} while ( data_req );

/* Find which one palert */
	if ( (staptr = pa2ew_list_find( header->serial )) == NULL ) {
	/* Serial should always larger than 0, if so send the update request */
		if ( header->serial > 0 ) {
			printf("palert2ew: %d not found in station list, maybe it's a new palert.\n", header->serial);
			return PA2EW_RECV_NEED_UPDATE;
		}
		else {
			/* printf("palert2ew: Recieve keep alive packet!\n"); */ /* Debug */
		}
	}
	else {
		if ( (ret = pa2ew_msgqueue_rawpacket( staptr, header + 1, header->length )) ) {
			if ( ret == 1 ) {
				if ( ++sync_errors >= 10 ) {
					sync_errors = 0;
					logit("e", "palert2ew: TCP connection sync error, reconnect!\n");
					if ( reconstruct_connect_sock() < 0 )
						return PA2EW_RECV_CONNECT_ERROR;
				}
			}
			else {
				sleep_ew(100);
			}
		}
		else {
			sync_errors = 0;
		}
	}

	return PA2EW_RECV_NORMAL;
}

/*
 * reconstruct_connect_sock() - Reconstruct the socket connect to the Palert server.
 */
static int reconstruct_connect_sock( void )
{
	static uint8_t count = 0;

/* */
	if ( ClientSocket != -1 )
		close(ClientSocket);
/* Do until we success getting socket or exceed 100 times */
	while ( (ClientSocket = construct_connect_sock( _ServerIP, _ServerPort )) == -1 ) {
	/* Try 10 times */
		if ( ++count > RECONNECT_TIMES ) {
			logit("e", "palert2ew: Reconstruct socket failed; exiting!\n");
			return -1;
		}
	/* Waiting for a while */
		sleep_ew(RECONNECT_INTERVAL_MSEC);
	}
	logit("t", "palert2ew: Reconstruct socket success!\n");
	count = 0;

	return ClientSocket;
}

/*
 *  construct_connect_sock() - Construct the socket connect to the Palert server.
 */
static int construct_connect_sock( const char *ip, const char *port )
{
	int result   = -1;
	int sock_opt = 1;

	struct addrinfo  hints;
	struct addrinfo *servinfo, *p;
	struct timeval   timeout;          /* Socket connect timeout */

/* Initialize the address info structure */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
/* Get IP & port information */
	if ( getaddrinfo(ip, port, &hints, &servinfo) ) {
		logit("e", "palert2ew: Get connection address info error!\n");
		return -1;
	}

/* Setup socket */
	for ( p = servinfo; p != NULL; p = p->ai_next) {
		if ( (result = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1 ) {
			logit("e", "palert2ew: Construct Palert connection socket error!\n");
			return -1;
		}
	/* Set connection timeout to 15 seconds */
		timeout.tv_sec  = 15;
		timeout.tv_usec = 0;
	/* Setup characteristics of socket */
		setsockopt(result, IPPROTO_TCP, TCP_QUICKACK, &sock_opt, sizeof(sock_opt));
		setsockopt(result, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt));
		setsockopt(result, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	/* Connect to the Palert server if we are using dependent client mode */
		if ( connect(result, p->ai_addr, p->ai_addrlen) < 0 ) {
			logit("e", "palert2ew: Connect to Palert server error!\n");
			continue;
		}

		break;
	}
	freeaddrinfo(servinfo);

	if ( p != NULL ) {
		logit("o", "palert2ew: Connection to Palert server %s success!\n", ip);
	}
	else {
		logit("e", "palert2ew: Construct Palert connection socket failed!\n");
		close(result);
		result = -1;
	}

	return result;
}
