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
#define FW_PCK_HEADER_LENGTH     4
#define RETRY_TIMES_LIMIT        5
#define RECONNECT_TIMES_LIMIT    10
#define RECONNECT_INTERVAL_MSEC  15000
#define SOCKET_RCVBUFFER_LENGTH  1048576

/* */
typedef struct {
	uint16_t serial;
	uint16_t length;
	uint8_t  recv_buffer[PA2EW_RECV_BUFFER_LENGTH];
} FW_PCK;

/* */
static void flush_sock_buffer( const int );
static int  reconstruct_connect_sock( void );
static int  construct_connect_sock( const char *, const char * );

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
	/* */
		BufferSize = sizeof(LABELED_RECV_BUFFER) > sizeof(FW_PCK) ? sizeof(LABELED_RECV_BUFFER) : sizeof(FW_PCK);
		Buffer = (uint8_t *)calloc(1, BufferSize + 1);  /* Plus one just in case */
	}

	return ClientSocket;
}

/*
 * pa2ew_client_end() - End process of Palert client.
 */
void pa2ew_client_end( void )
{
	logit("o", "palert2ew: Closing the connections to Palert server!\n");
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
 *                         and send it to the queue.
 */
int pa2ew_client_stream( void )
{
	static LABELED_RECV_BUFFER *lrbuf = NULL;
	static FW_PCK              *fwptr = NULL;
	static uint8_t              sync_errors = 0;

	int       ret       = 0;
	int       retry     = 0;
	int       data_read = 0;
	int       data_req  = FW_PCK_HEADER_LENGTH;
	_STAINFO *staptr    = NULL;

/* Try to align the two different data structure pointer */
	if ( (uint8_t *)lrbuf != Buffer && (uint8_t *)fwptr != Buffer ) {
	/* */
		lrbuf = (LABELED_RECV_BUFFER *)Buffer;
		fwptr = (FW_PCK *)Buffer;
	/* */
		if ( lrbuf->recv_buffer > fwptr->recv_buffer )
			fwptr = (FW_PCK *)(Buffer + (lrbuf->recv_buffer - fwptr->recv_buffer));
		else
			lrbuf = (LABELED_RECV_BUFFER *)(Buffer + (fwptr->recv_buffer - lrbuf->recv_buffer));
	}
/* Reset the header of forward packet, just in case! */
	fwptr->serial = 0;
	fwptr->length = 0;
/* */
	do {
		printf("Recv: %p, read %d bytes, req %d bytes, total %d bytes.\n", fwptr, data_read, data_req, fwptr->length);
		if ( (ret = recv(ClientSocket, (uint8_t *)fwptr + data_read, data_req, 0)) <= 0 ) {
			if ( errno == EINTR ) {
				sleep_ew(10);
			}
			else if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT ) {
				logit("et", "palert2ew: Receiving from Palert server is timeout, retry #%d...\n", ++retry);
				if ( retry >= RETRY_TIMES_LIMIT ) {
					logit("et", "palert2ew: Retry over %d time(s), reconnect...\n", retry);
					goto reconnect;
				}
				sleep_ew(RECONNECT_INTERVAL_MSEC);
			}
			else if ( ret == 0 ) {
				logit("et", "palert2ew: Connection to Palert server is closed, reconnect...\n");
				goto reconnect;
			}
			else {
				logit("et", "palert2ew: Fatal error on Palert server, exiting this session!\n");
				return PA2EW_RECV_FATAL_ERROR;
			}
			continue;
		}
	/* */
		if ( (data_read += ret) >= FW_PCK_HEADER_LENGTH )
			data_req = fwptr->length + FW_PCK_HEADER_LENGTH - data_read;
	} while ( data_req > 0 );

/* Serial should always larger than 0, if so send the update request */
	if ( fwptr->serial ) {
	/* Find which one palert */
		if ( (staptr = pa2ew_list_find( fwptr->serial )) ) {
			ret = fwptr->length;
		/* This should be done after the command above 'cause it will effect the length's memory space */
			lrbuf->label.staptr = staptr;
		/* Packet type temporary fixed on 1 */
			if ( pa2ew_msgqueue_rawpacket( lrbuf, ret, 1, PA2EW_GEN_MSG_LOGO_BY_SRC( PA2EW_MSG_CLIENT_STREAM ) ) ) {
				if ( ++sync_errors >= PA2EW_TCP_SYNC_ERR_LIMIT ) {
					logit("et", "palert2ew: TCP connection sync error, flushing the buffer...\n");
					flush_sock_buffer( ClientSocket );
					pa2ew_msgqueue_lastbufs_reset();
					sync_errors = 0;
				}
			}
			else {
				sync_errors = 0;
			}
		}
		else {
			printf("palert2ew: Serial(%d) not found in station list, maybe it's a new palert.\n", fwptr->serial);
			return PA2EW_RECV_NEED_UPDATE;
		}
	}
/*
	else {
		printf("palert2ew: Recieve keep alive packet!\n");
	}
*/
	return PA2EW_RECV_NORMAL;
/* */
reconnect:
	pa2ew_msgqueue_lastbufs_reset();
	if ( reconstruct_connect_sock() < 0 )
		return PA2EW_RECV_CONNECT_ERROR;
	else
		return PA2EW_RECV_NORMAL;
}

/*
 * flush_sock_buffer()
 */
static void flush_sock_buffer( const int sock )
{
	int      times;
	uint8_t *buf = (uint8_t *)malloc(SOCKET_RCVBUFFER_LENGTH + 1);

	if ( sock > 0 && buf ) {
		times = 0;
		do {
			logit("ot", "palert2ew: NOTICE! Flushing socket(%d) buffer #%d...\n", sock, ++times);
		} while ( recv(sock, buf, SOCKET_RCVBUFFER_LENGTH, 0) >= SOCKET_RCVBUFFER_LENGTH );
	}
/* */
	if ( buf )
		free(buf);

	return;
}

/*
 * reconstruct_connect_sock() - Reconstruct the socket connect to the Palert server.
 */
static int reconstruct_connect_sock( void )
{
	static uint8_t count = 0;
/* */
	int sock = ClientSocket;

/* Do until we success getting socket or exceed RECONNECT_TIMES_LIMIT */
	while ( (ClientSocket = construct_connect_sock( _ServerIP, _ServerPort )) == -1 ) {
	/* Try RECONNECT_TIMES_LIMIT */
		if ( ++count > RECONNECT_TIMES_LIMIT ) {
			logit("et", "palert2ew: Reconstruct socket failed; exiting this session!\n");
			ClientSocket = sock;
			return -1;
		}
	/* Waiting for a while */
		sleep_ew(RECONNECT_INTERVAL_MSEC);
	}
	logit("ot", "palert2ew: Reconstruct socket success!\n");
	count = 0;
/* */
	if ( sock > 0 )
		close(sock);

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
		logit("et", "palert2ew: Get connection address info error!\n");
		return -1;
	}

/* Setup socket */
	for ( p = servinfo; p != NULL; p = p->ai_next) {
		if ( (result = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1 ) {
			logit("et", "palert2ew: Construct Palert connection socket error!\n");
			return -1;
		}
	/* Set connection timeout to 15 seconds */
		timeout.tv_sec  = 15;
		timeout.tv_usec = 0;
	/* Setup characteristics of socket */
		setsockopt(result, IPPROTO_TCP, TCP_QUICKACK, &sock_opt, sizeof(sock_opt));
		setsockopt(result, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	/* Set recv. buffer size */
		sock_opt = SOCKET_RCVBUFFER_LENGTH;
		setsockopt(result, SOL_SOCKET, SO_RCVBUFFORCE, &sock_opt, sizeof(sock_opt));
	/* Connect to the Palert server if we are using dependent client mode */
		if ( connect(result, p->ai_addr, p->ai_addrlen) < 0 ) {
			logit("et", "palert2ew: Connect to Palert server error!\n");
			continue;
		}

		break;
	}
	freeaddrinfo(servinfo);

	if ( p != NULL ) {
		logit("ot", "palert2ew: Connection to Palert server %s success(sock: %d)!\n", ip, result);
	}
	else {
		logit("et", "palert2ew: Construct Palert connection socket failed!\n");
		close(result);
		result = -1;
	}

	return result;
}
