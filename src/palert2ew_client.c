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

/* */
#define FW_PCK_HEADER_LENGTH    4
#define RECONNECT_TIMES         10
#define RECONNECT_INTERVAL_MSEC 15000

/* */
typedef struct {
	uint16_t serial;
	uint16_t length;
	uint8_t  recv_buffer[PA2EW_RECV_BUFFER_LENGTH];
} FW_PCK;

/* */
static int reconstruct_connect_sock( void );
static int construct_connect_sock( const char *, const char * );

/* */
static volatile int  ClientSocket = -1;
static const char   *_ServerIP    = NULL;
static const char   *_ServerPort  = NULL;
static uint8_t      *Buffer       = NULL;
static size_t        BufferSize   = 0;

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
/* Initialize the receiving buffer */
	if ( Buffer == NULL ) {
		if ( sizeof(LABELED_RECV_BUFFER) > sizeof(FW_PCK) )
			BufferSize = sizeof(LABELED_RECV_BUFFER);
		else
			BufferSize = sizeof(FW_PCK);
	/* */
		Buffer = (uint8_t *)malloc(BufferSize);
	}

	return ClientSocket;
}

/*
 * pa2ew_client_end() - End process of Palert client.
 */
void pa2ew_client_end( void )
{
	close(ClientSocket);
/* */
	if ( Buffer != NULL ) {
		free(Buffer);
		Buffer = NULL;
	}

	return;
}

/*
 * pa2ew_client_stream() - Receive the messages from the socket of forward server
 *                         and send it to the MessageStacker.
 */
int pa2ew_client_stream( void )
{
	static LABELED_RECV_BUFFER *lrbuf = NULL;
	static FW_PCK              *fwptr = NULL;
	static size_t               offset = 0;
	static uint8_t              sync_errors = 0;

	int ret       = 0;
	int data_read = 0;
	int data_req  = FW_PCK_HEADER_LENGTH;

/* Try to align the two different data structure pointer */
	if ( lrbuf == NULL || fwptr == NULL ) {
	/* */
		lrbuf  = (LABELED_RECV_BUFFER *)Buffer;
		fwptr  = (FW_PCK *)Buffer;
	/* */
		if ( lrbuf->recv_buffer > fwptr->recv_buffer )
			fwptr = (FW_PCK *)(Buffer + (lrbuf->recv_buffer - fwptr->recv_buffer));
		else
			lrbuf = (LABELED_RECV_BUFFER *)(Buffer + (fwptr->recv_buffer - lrbuf->recv_buffer));
	/* */
		offset = lrbuf->recv_buffer - (uint8_t *)lrbuf;
	}

/* */
	memset(Buffer, 0, BufferSize);
	do {
		if ( (ret = recv(ClientSocket, (uint8_t *)fwptr + data_read, data_req, 0)) <= 0 ) {
			if ( errno == EINTR ) {
				sleep_ew(25);
			}
			else if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT ) {
				logit("e", "palert2ew: Receiving from Palert server is timeout, retry...\n");
				sleep_ew(RECONNECT_INTERVAL_MSEC);
			}
			else if ( ret == 0 ) {
				logit("e", "palert2ew: Connection to Palert server is closed, reconnect...\n");
				if ( reconstruct_connect_sock() < 0 )
					return PA2EW_RECV_CONNECT_ERROR;

				data_read = 0;
				data_req  = FW_PCK_HEADER_LENGTH;
			}
			else {
				logit("e", "palert2ew: Fatal error on Palert server, exiting this session!\n");
				return PA2EW_RECV_FATAL_ERROR;
			}
			continue;
		}
	/* */
		if ( (data_read += ret) >= FW_PCK_HEADER_LENGTH ) {
			data_req = fwptr->length + FW_PCK_HEADER_LENGTH - data_read;
		}
	} while ( data_req > 0 );

/* Serial should always larger than 0, if so send the update request */
	if ( fwptr->serial > 0 ) {
		if ( pa2ew_list_find( fwptr->serial ) ) {
			ret = fwptr->length + offset;
			lrbuf->label.serial = fwptr->serial;
		/* Packet type temporary fixed on 1 */
			if ( pa2ew_msgqueue_rawpacket( lrbuf, ret, 1, PA2EW_GEN_MSG_LOGO_BY_SRC( PA2EW_MSG_CLIENT_STREAM ) ) ) {
				if ( ++sync_errors >= PA2EW_TCP_SYNC_ERR_LIMIT ) {
					logit("e", "palert2ew: TCP connection sync error, reconnect!\n");
					if ( reconstruct_connect_sock() < 0 )
					return PA2EW_RECV_CONNECT_ERROR;
					sync_errors = 0;
				}
			}
			else {
				sync_errors = 0;
			}
		}
		else {
			printf("palert2ew: Serial(%d) not found in station list, maybe it's a new palert.\n", fwptr->serial);
		}
	}
	else {
		/* printf("palert2ew: Recieve keep alive packet!\n"); */ /* Debug */
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
	if ( ClientSocket != -1 ) {
		close(ClientSocket);
		sleep_ew(2000);
	}
/* Do until we success getting socket or exceed RECONNECT_TIMES */
	while ( (ClientSocket = construct_connect_sock( _ServerIP, _ServerPort )) == -1 ) {
	/* Try RECONNECT_TIMES */
		if ( ++count > RECONNECT_TIMES ) {
			logit("e", "palert2ew: Reconstruct socket failed; exiting this session!\n");
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
		logit("o", "palert2ew: Connection to Palert server %s success(sock: %d)!\n", ip, result);
	}
	else {
		logit("e", "palert2ew: Construct Palert connection socket failed!\n");
		close(result);
		result = -1;
	}

	return result;
}
