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
#include <palert2ew_misc.h>
#include <palert2ew_msg_queue.h>

/* */
#define FW_PCK_HEADER_LENGTH     16
#define RETRY_TIMES_LIMIT        5
#define RECONNECT_TIMES_LIMIT    10
#define RECONNECT_INTERVAL_MSEC  PA2EW_RECONNECT_INTERVAL
#define SOCKET_RCVBUFFER_LENGTH  1232896  /* It comes from 1024 * 1204 */

/* */
typedef struct {
	uint16_t serial;
	uint16_t length;
	uint32_t seq;
	uint16_t packmode;
	uint8_t  padding[3];
	int8_t   tzoffset;
	uint8_t  stratum;
	uint8_t  crc8;
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
	ClientSocket = -1;
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
	static uint32_t             recv_seq = 0;

	int       ret       = 0;
	int       retry     = 0;
	int       checked   = 0;
	int       data_read = 0;
	int       data_req  = FW_PCK_HEADER_LENGTH;
	uint16_t  packmode  = 0;
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
/* */
	do {
		if ( (ret = recv(ClientSocket, (uint8_t *)fwptr + data_read, data_req, 0)) <= 0 ) {
			if ( errno == EINTR ) {
				sleep_ew(100);
			}
			else if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT ) {
				logit("et", "palert2ew: Receiving from Palert server is timeout, retry #%d...\n", ++retry);
				if ( retry >= RETRY_TIMES_LIMIT ) {
					logit("et", "palert2ew: Retry over %d time(s), reconnecting...\n", retry);
					goto reconnect;
				}
				sleep_ew(RECONNECT_INTERVAL_MSEC);
			}
			else if ( ret == 0 ) {
				logit("et", "palert2ew: Connection to Palert server is closed, reconnecting...\n");
				goto reconnect;
			}
			else {
				logit("et", "palert2ew: Fatal error on Palert server, exiting this session!\n");
				return PA2EW_RECV_FATAL_ERROR;
			}
			continue;
		}
	/* */
		if ( (data_read += ret) >= FW_PCK_HEADER_LENGTH ) {
			if ( !checked ) {
				if ( fwptr->seq != recv_seq && pa2ew_crc8_cal( fwptr, FW_PCK_HEADER_LENGTH ) ) {
					logit("et", "palert2ew: TCP connection sync error, flushing the buffer...\n");
					flush_sock_buffer( ClientSocket );
					pa2ew_msgqueue_lastbufs_reset( NULL );
				/* */
					if ( ++sync_errors >= PA2EW_TCP_SYNC_ERR_LIMIT ) {
						logit("et", "palert2ew: TCP connection sync error over %u times, reconnecting...\n", sync_errors);
						sync_errors = 0;
						goto reconnect;
					}
				/* */
					return PA2EW_RECV_NORMAL;
				}
			/* */
				checked  = 1;
				recv_seq = fwptr->seq;
			}
			data_req = fwptr->length + FW_PCK_HEADER_LENGTH - data_read;
		}
	} while ( data_req > 0 );

/* */
	recv_seq++;
/* Serial should always larger than 0 & ignore keep-alive(serial = 0) packet */
	if ( fwptr->serial ) {
	/* Find which one palert */
		if ( (staptr = pa2ew_list_find( fwptr->serial )) ) {
			ret      = fwptr->length;
			packmode = fwptr->packmode;
		/* Get the time shift in seconds between UTC & palert timezone */
			staptr->timeshift = -(fwptr->tzoffset * 3600);
		/* These should be done after the statement above 'cause it use the same memory space */
			lrbuf->label.staptr   = staptr;
			lrbuf->label.packmode = packmode;
		/* Packet type should be provided by server side */
			if ( pa2ew_msgqueue_rawpacket( lrbuf, ret, PA2EW_GEN_MSG_LOGO_BY_SRC( PA2EW_MSG_CLIENT_STREAM ) ) ) {
				logit("et", "palert2ew: Serial(%d) packet sync error, flushing the last buffer...\n", staptr->serial);
				pa2ew_msgqueue_lastbufs_reset( staptr );
			}
			sync_errors = 0;
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
	pa2ew_msgqueue_lastbufs_reset( NULL );
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
	if ( ClientSocket > 0 ) {
		close(ClientSocket);
		sleep_ew(RECONNECT_INTERVAL_MSEC);
	}
/* Do until we success getting socket or exceed RECONNECT_TIMES_LIMIT */
	while ( (ClientSocket = construct_connect_sock( _ServerIP, _ServerPort )) == -1 ) {
	/* Try RECONNECT_TIMES_LIMIT */
		if ( ++count > RECONNECT_TIMES_LIMIT ) {
			logit("et", "palert2ew: Reconstruct socket failed; exiting this session!\n");
			ClientSocket = -1;
			return ClientSocket;
		}
	/* Waiting for a while */
		sleep_ew(RECONNECT_INTERVAL_MSEC);
	}
	logit("ot", "palert2ew: Reconstruct socket success!\n");
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
		logit("et", "palert2ew: Get connection address info error!\n");
		return -1;
	}

/* Setup socket */
	for ( p = servinfo; p != NULL; p = p->ai_next ) {
		if ( (result = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1 ) {
			logit("et", "palert2ew: Construct Palert connection socket error!\n");
			return -1;
		}
	/* Set connection timeout to 15 seconds */
		timeout.tv_sec  = 15;
		timeout.tv_usec = 0;
	/* Setup characteristics of socket */
		if ( setsockopt(result, IPPROTO_TCP, TCP_QUICKACK, &sock_opt, sizeof(sock_opt)) == -1 ) {
			logit("et", "palert2ew: Construct Palert server connection socket(%s) error(setsockopt: TCP_QUICKACK)!\n", port);
			goto err_return;
		}
		if ( setsockopt(result, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) == -1 ) {
			logit("et", "palert2ew: Construct Palert server connection socket(%s) error(setsockopt: SO_RCVTIMEO)!\n", port);
			goto err_return;
		}
	/* Set recv. buffer size */
		sock_opt = SOCKET_RCVBUFFER_LENGTH;
		if ( setsockopt(result, SOL_SOCKET, SO_RCVBUFFORCE, &sock_opt, sizeof(sock_opt)) == -1 ) {
			logit("et", "palert2ew: Construct Palert server connection socket(%s) error(setsockopt: SO_RCVBUFFORCE)!\n", port);
			logit("et", "palert2ew: Work under system default receiving buffer size!!\n");
		}
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
		goto err_return;
	}

	return result;

/* Return for error happened */
err_return:
	close(result);
	return -1;
}
