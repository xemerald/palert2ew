/*
 *
 */
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <unistd.h>
#include <errno.h>
/* Network related header include */
#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <palert2ew.h>
#include <palert2ew_misc.h>
#include <palert2ew_list.h>
#include <palert2ew_server.h>
#include <palert2ew_msg_queue.h>

/* Internal function prototype */
static int construct_listen_sock( const char * );
static int accept_palert_raw( void );
static int find_which_station( void *, CONNDESCRIP *, int );
static int compare_serial( const void *, const void * );
static int eval_threadnum( int );

/* Define global variables */
volatile int              MaxStationNum = 0;
volatile int              AcceptEpoll   = 0;
static volatile int       AcceptSocket  = -1;
static volatile int       ThreadsNumber = 0;
static PALERT_THREAD_SET *ThreadSets    = NULL;
static CONNDESCRIP       *PalertConns   = NULL;


/*
 * pa2ew_server_init() - Initialize the independent Palert server &
 *                       return the needed threads number.
 */
int pa2ew_server_init( const int max_stations, const char *port )
{
	int i;

/* Setup constants */
	AcceptEpoll   = epoll_create(2);
	MaxStationNum = max_stations;
	ThreadsNumber = eval_threadnum( max_stations );
	ThreadSets    = calloc(ThreadsNumber, sizeof(PALERT_THREAD_SET));
/* Create epoll sets */
	for ( i = 0; i < ThreadsNumber; i++ ) {
		ThreadSets[i].epoll_fd = epoll_create(PA2EW_MAX_PALERTS_PER_THREAD);
		ThreadSets[i].buffer   = calloc(1, sizeof(LABELED_RECV_BUFFER));
		ThreadSets[i].evts     = calloc(PA2EW_MAX_PALERTS_PER_THREAD, sizeof(struct epoll_event));
	}
/* Construct the accept socket for normal stream */
	AcceptSocket = pa2ew_server_common_init(
		max_stations, port, AcceptEpoll, &PalertConns, accept_palert_raw
	);
	if ( AcceptSocket <= 0 )
		return -1;

	return ThreadsNumber;
}

/*
 *  pa2ew_server_end() - End process of Palert server.
 */
void pa2ew_server_end( void )
{
	int i;

/* */
	logit("o", "palert2ew: Closing all the connections of Palerts!\n");
	if ( AcceptSocket != -1 )
		close(AcceptSocket);
	if ( AcceptEpoll )
		close(AcceptEpoll);
/* Closing connections of Palerts */
	if ( PalertConns != NULL ) {
		for ( i = 0; i < MaxStationNum; i++ )
			pa2ew_server_common_pconnect_close( (PalertConns + i), ThreadSets[i % ThreadsNumber].epoll_fd );
		free(PalertConns);
	}
/* Free epoll & readevts */
	if ( ThreadSets != NULL ) {
		for ( i = 0; i < ThreadsNumber; i++ ) {
			close(ThreadSets[i].epoll_fd);
			free(ThreadSets[i].buffer);
			free(ThreadSets[i].evts);
		}
		free(ThreadSets);
	}

	return;
}

/*
 *
 */
void pa2ew_server_pconnect_walk( void (*action)(const void *, const int, void *), void *arg )
{
	pa2ew_server_common_pconnect_walk( PalertConns, MaxStationNum, action, arg );

	return;
}

/*
 *
 */
int pa2ew_server_palerts_accept( const int msec )
{
	int                i, nready;
	struct epoll_event evts[LISTENQ];
	int              (*accept_func)( void ) = NULL;

/* */
	nready = epoll_wait(AcceptEpoll, evts, LISTENQ, msec);
	for ( i = 0; i < nready; i++ ) {
		if ( evts[i].events & EPOLLIN ) {
			accept_func = (int (*)( void ))evts[i].data.ptr;
			accept_func();
		}
	}

	return nready;
}

/*
 * pa2ew_server_pconnect_check() - Check connections of all Palerts.
 */
int pa2ew_server_pconnect_check( void )
{
	int            i, result = 0;
	double         time_now;
	CONNDESCRIP   *conn = PalertConns;

/* */
	if ( conn != NULL ) {
	/* */
		time_now = pa2ew_misc_timenow_get();
		for ( i = 0; i < MaxStationNum; i++, conn++ ) {
			if ( conn->sock != -1 ) {
				if ( (time_now - conn->last_act) > (double)PA2EW_IDLE_THRESHOLD ) {
					logit(
						"t", "palert2ew: Connection from %s idle over %d seconds, close connection!\n",
						conn->ip, PA2EW_IDLE_THRESHOLD
					);
					pa2ew_server_common_pconnect_close( conn, ThreadSets[i % ThreadsNumber].epoll_fd );
				}
				if ( conn->label.serial )
					result++;
			}
		}
	}

	return result;
}

/*
 * pa2ew_server_pconnect_find()
 */
CONNDESCRIP *pa2ew_server_pconnect_find( const uint16_t serial )
{
	return pa2ew_server_common_pconnect_find( PalertConns, MaxStationNum, serial );
}

/*
 *
 */
int pa2ew_server_common_init(
	const int max_stations, const char *port, const int epoll, CONNDESCRIP **conn, int (*accept_func)( void )
) {
	int                i;
	int                result;
	struct epoll_event connevt;

/* Allocating Palerts' connection descriptors */
	if ( (*conn = calloc(max_stations, sizeof(CONNDESCRIP))) == NULL ) {
		logit("e", "palert2ew: Error allocating connection descriptors for port: %s!\n", port);
		return -1;
	}
	else {
		logit("", "palert2ew: %d connection descriptors allocated for port: %s!\n", max_stations, port);
		for ( i = 0; i < max_stations; i++ )
			RESET_CONNDESCRIP( *conn + i );
	}
/* Construct the accept socket */
	if ( (result = construct_listen_sock( port )) == -1 )
		return -2;
/* */
	connevt.events   = EPOLLIN | EPOLLERR;
	connevt.data.ptr = accept_func;
	epoll_ctl(epoll, EPOLL_CTL_ADD, result, &connevt);

	return result;
}

/*
 * pa2ew_server_common_accept() -
 */
CONNDESCRIP pa2ew_server_common_accept( const int sock )
{
	CONNDESCRIP             result;
	struct sockaddr_storage cliaddr;
	struct sockaddr_in     *cli4_ptr = (struct sockaddr_in *)&cliaddr;
	struct sockaddr_in6    *cli6_ptr = (struct sockaddr_in6 *)&cliaddr;
	unsigned int            clilen   = sizeof(cliaddr);

/* */
	RESET_CONNDESCRIP( &result );
/* */
	if ( (result.sock = accept(sock, (struct sockaddr *)&cliaddr, &clilen)) == -1 ) {
		printf("palert2ew: Accepted new Palert's connection from socket: %d error!\n", sock);
		return result;
	}

	switch ( cliaddr.ss_family ) {
	case AF_INET:
		inet_ntop(cliaddr.ss_family, &cli4_ptr->sin_addr, result.ip, INET6_ADDRSTRLEN);
		result.port = (int)cli4_ptr->sin_port;
		break;
	case AF_INET6:
		inet_ntop(cliaddr.ss_family, &cli6_ptr->sin6_addr, result.ip, INET6_ADDRSTRLEN);
		result.port = (int)cli6_ptr->sin6_port;
		break;
	default:
		printf("palert2ew: Accept socket internet type unknown, drop it!\n");
		result.sock = -1;
		return result;
		break;
	}
/* */
	result.last_act = pa2ew_misc_timenow_get();

	return result;
}

/*
 * pa2ew_server_common_pconnect_walk() -
 */
void pa2ew_server_common_pconnect_walk(
	const CONNDESCRIP *conn, const int conn_num, void (*action)(const void *, const int, void *), void *arg
) {
	int i;

	if ( conn && conn_num ) {
		for ( i = 0; i < conn_num; i++ )
			action( conn + i, i, arg );
	}

	return;
}

/*
 * pa2ew_server_common_pconnect_find() -
 */
CONNDESCRIP *pa2ew_server_common_pconnect_find( const CONNDESCRIP *conn, const int conn_num, const uint16_t serial )
{
	CONNDESCRIP *result = NULL;
	CONNDESCRIP  key;
	size_t       num = conn_num;

	if ( conn && num ) {
		key.label.serial = serial;
		result = lfind(&key, conn, &num, sizeof(CONNDESCRIP), compare_serial);
	}

	return result;
}

/*
 * pa2ew_server_common_pconnect_close() - Close the connect of the Palert.
 */
void pa2ew_server_common_pconnect_close( CONNDESCRIP *conn, const int epoll )
{
	if ( conn->sock != -1 ) {
		struct epoll_event tmpev;
		tmpev.events   = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
		tmpev.data.ptr = conn;
	/* Raw connection */
		epoll_ctl(epoll, EPOLL_CTL_DEL, conn->sock, &tmpev);
	/* */
		close(conn->sock);
		RESET_CONNDESCRIP( conn );
	}
	return;
}

/*
 * pa2ew_server_proc() - Read the streaming data from each Palert and put it
 *                       into queue.
 */
int pa2ew_server_proc( const int countindex, const int msec )
{
	int    i, nready;
	double time_now;
	_Bool  need_update = 0;
/* */
	int                  epoll  = ThreadSets[countindex].epoll_fd;
	LABELED_RECV_BUFFER *buffer = (LABELED_RECV_BUFFER *)ThreadSets[countindex].buffer;
	struct epoll_event  *evts   = ThreadSets[countindex].evts;
	const size_t         offset = buffer->recv_buffer - (uint8_t *)buffer;

/* Wait the epoll for msec minisec */
	if ( (nready = epoll_wait(epoll, evts, PA2EW_MAX_PALERTS_PER_THREAD, msec)) ) {
		time_now = pa2ew_misc_timenow_get();
	/* There is some incoming data from socket */
		for ( i = 0; i < nready; i++ ) {
			if ( evts[i].events & EPOLLIN || evts[i].events & EPOLLRDHUP || evts[i].events & EPOLLERR ) {
				int          ret  = 0;
				CONNDESCRIP *conn = (CONNDESCRIP *)evts[i].data.ptr;
			/* */
				if ( (ret = recv(conn->sock, buffer->recv_buffer, PA2EW_RECV_BUFFER_LENGTH, 0)) <= 0 ) {
					printf(
						"palert2ew: Palert IP:%s, read length:%d, errno:%d(%s), close connection!\n",
						conn->ip, ret, errno, strerror(errno)
					);
					pa2ew_server_common_pconnect_close( conn, epoll );
				}
				else {
					if ( conn->label.serial ) {
					/* Just send it to the main queue */
						buffer->label.serial = conn->label.serial;
						if ( pa2ew_msgqueue_rawpacket( buffer, ret + offset, conn->packet_type ) ) {
							if ( ++conn->sync_errors >= PA2EW_TCP_SYNC_ERR_LIMIT ) {
								logit(
									"e","palert2ew: Palert %d TCP connection sync error, close connection!\n",
									conn->label.serial
								);
								pa2ew_server_common_pconnect_close( conn, epoll );
							}
						}
						else {
							conn->sync_errors = 0;
						}
					}
					else if ( ret >= 200 ) {
						if ( !palert_check_sync_common( buffer->recv_buffer ) ) {
							printf("palert2ew: Palert IP:%s sync failure, close connection!\n", conn->ip);
							pa2ew_server_common_pconnect_close( conn, epoll );
						}
						else {
							need_update = find_which_station( buffer, conn, epoll );
						}
					}
					else {
					/* Receive data not enough, close connection */
						printf(
							"palert2ew: Palert IP:%s send data not enough to check, close connection!\n",
							conn->ip
						);
						pa2ew_server_common_pconnect_close( conn, epoll );
					}
				}
			/* */
				conn->last_act = time_now;
			}
		}
	}
/* */
	return need_update ? PA2EW_RECV_NEED_UPDATE : PA2EW_RECV_NORMAL;
}

/*
 * construct_listen_sock() - Construct Palert listening connection socket.
 */
static int construct_listen_sock( const char *port )
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
	hints.ai_flags    = AI_PASSIVE;
/* Get port information */
	if ( getaddrinfo(NULL, port, &hints, &servinfo) ) {
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
	/* Bind socket to listening port */
		if ( bind(result, p->ai_addr, p->ai_addrlen) == -1 ) {
			logit("e", "palert2ew: Bind Palert listening socket error!\n");
			continue;
		}

		break;
	}
	freeaddrinfo(servinfo);

	if ( p != NULL ) {
	/* Listen on socket if we are using independent server mode */
		if ( listen(result, LISTENQ) ) {
			logit("e", "palert2ew: Listen Palert connection socket error!\n");
		}
		else {
			logit("o", "palert2ew: Listen Palert connection socket: %d ready!\n", result);
			return result;
		}
	}
	else {
		logit("e", "palert2ew: Construct Palert connection socket failed!\n");
	}

	close(result);
	return -1;
}


/*
 * accept_palert_raw() - Accept the connection of Palerts then add it
 *                       into connection descriptor and EpollFd.
 */
static int accept_palert_raw( void )
{
	int                i;
	CONNDESCRIP       *conn = NULL;
	CONNDESCRIP        tmpconn;
	struct epoll_event acceptevt;

/* */
	acceptevt.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
	acceptevt.data.ptr = NULL;
/* */
	tmpconn = pa2ew_server_common_accept( AcceptSocket );
	if ( tmpconn.sock < 0 )
		return -1;
/* Find and save to an empty Palert connection */
	for ( i = 0, conn = PalertConns; i < MaxStationNum; i++, conn++ ) {
		if ( conn->sock == -1 ) {
			*conn = tmpconn;
			acceptevt.data.ptr = conn;
			epoll_ctl(ThreadSets[i % ThreadsNumber].epoll_fd, EPOLL_CTL_ADD, conn->sock, &acceptevt);
			printf("palert2ew: New Palert connection from %s:%d.\n", conn->ip, conn->port);
			break;
		}
	}

	if ( i == MaxStationNum ) {
		printf("palert2ew: Palert connection is full. Drop connection from %s:%d.\n", tmpconn.ip, tmpconn.port);
		close(tmpconn.sock);
		return -2;
	}

	return 0;
}

/*
 *
 */
static int find_which_station( void *buffer, CONNDESCRIP *conn, int epoll )
{
	int       result = 0;
	uint16_t  serial = palert_get_serial_common( ((LABELED_RECV_BUFFER *)buffer)->recv_buffer );
	_STAINFO *staptr = pa2ew_list_find( serial );
/* */
	if ( !staptr ) {
	/* Not found in Palert table */
		printf("palert2ew: S/N %d not found in station list, maybe it's a new palert.\n", serial);
		result = -1;
	/* Drop the connection */
		pa2ew_server_common_pconnect_close( conn, epoll );
	}
	else {
	/* Checking for existing connection with the same serial...*/
		CONNDESCRIP *_conn = pa2ew_server_common_pconnect_find( PalertConns, MaxStationNum, serial );
		if ( _conn ) {
			int i = (_conn - PalertConns) % ThreadsNumber;
			pa2ew_server_common_pconnect_close( _conn, ThreadSets[i].epoll_fd );
		}
	/* */
		conn->label.serial = staptr->serial;
		conn->packet_type  = palert_get_packet_type_common( ((LABELED_RECV_BUFFER *)buffer)->recv_buffer );
		printf("palert2ew: Palert %s now online.\n", staptr->sta);
	}

	return result;
}

/*
 * compare_serial() - reverse version of compare function.
 */
static int compare_serial( const void *node_a, const void *node_b )
{
	int serial_a = ((CONNDESCRIP *)node_a)->label.serial;
	int serial_b = ((CONNDESCRIP *)node_b)->label.serial;

	if ( serial_a > serial_b )
		return -1;
	else if ( serial_a < serial_b )
		return 1;
	else
		return 0;
}

/*
 *
 */
static int eval_threadnum( int max_stations )
{
	int result;
/* */
	for ( result = 0; max_stations > 0; max_stations -= PA2EW_MAX_PALERTS_PER_THREAD )
		result++;

	return result;
}
