/**
 * @file palert2ew_server.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief
 * @date 2020-08-01
 *
 * @copyright Copyright (c) 2020
 *
 */

/**
 * @name Standard C header include
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <unistd.h>
#include <errno.h>

/**
 * @name Network related header include
 *
 */
#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

/**
 * @name Earthworm environment header include
 *
 */
#include <earthworm.h>

/**
 * @name Local header include
 *
 */
#include <libpalertc/libpalertc.h>
#include <palert2ew.h>
#include <palert2ew_misc.h>
#include <palert2ew_list.h>
#include <palert2ew_server.h>
#include <palert2ew_msg_queue.h>

/**
 * @name Internal functions' prototype
 *
 */
static int construct_listen_sock( const char * );
static int accept_palert_raw( void );
static int find_which_station( void *, CONNDESCRIP *, int );
static int find_palert_tzoffset( const PALERT_M1_HEADER * );

/**
 * @name Internal static variables
 *
 */
volatile int              AcceptEpoll   = 0;
static volatile int       AcceptSocket  = -1;
static volatile int       ThreadsNumber = 0;
static volatile int       MaxStationNum = 0;
static PALERT_THREAD_SET *ThreadSets    = NULL;
static CONNDESCRIP       *PalertConns   = NULL;

/**
 * @brief Initialize the independent Palert server & return the needed threads number.
 *
 * @param max_stations
 * @param port
 * @return int
 */
int pa2ew_server_init( const int max_stations, const char *port )
{
/* Setup constants */
	AcceptEpoll   = epoll_create(2);
	MaxStationNum = max_stations;
	ThreadsNumber = pa2ew_recv_thrdnum_eval( max_stations, PA2EW_RECV_SERVER_ON );
	ThreadSets    = calloc(ThreadsNumber, sizeof(PALERT_THREAD_SET));
/* Create epoll sets */
	for ( int i = 0; i < ThreadsNumber; i++ ) {
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

/**
 * @brief End process of Palert server.
 *
 */
void pa2ew_server_end( void )
{
/* */
	logit("o", "palert2ew: Closing all the connections of Palerts!\n");
	if ( AcceptSocket > 0 )
		close(AcceptSocket);
	if ( AcceptEpoll )
		close(AcceptEpoll);
/* Closing connections of Palerts */
	if ( PalertConns != NULL ) {
		for ( int i = 0; i < MaxStationNum; i++ )
			pa2ew_server_common_pconnect_close( (PalertConns + i), ThreadSets[i % ThreadsNumber].epoll_fd );
		free(PalertConns);
	}
/* Free epoll & readevts */
	if ( ThreadSets != NULL ) {
		for ( int i = 0; i < ThreadsNumber; i++ ) {
			close(ThreadSets[i].epoll_fd);
			free(ThreadSets[i].buffer);
			free(ThreadSets[i].evts);
		}
		free(ThreadSets);
	}

	return;
}

/**
 * @brief
 *
 * @param action
 * @param arg
 */
void pa2ew_server_pconnect_walk( void (*action)(const void *, const int, void *), void *arg )
{
	pa2ew_server_common_pconnect_walk( PalertConns, MaxStationNum, action, arg );

	return;
}

/**
 * @brief
 *
 * @param msec
 * @return int
 */
int pa2ew_server_palerts_accept( const int msec )
{
	int                nready = 0;
	struct epoll_event evts[LISTENQ];
	int              (*accept_func)( void ) = NULL;

/* */
	if ( msec ) {
		nready = epoll_wait(AcceptEpoll, evts, LISTENQ, msec);
		for ( int i = 0; i < nready; i++ ) {
			if ( evts[i].events & EPOLLIN ) {
				accept_func = (int (*)( void ))evts[i].data.ptr;
				if ( accept_func() == -1 )
					return -1;
			}
		}
	}

	return nready;
}

/**
 * @brief Check connections of all Palerts.
 *
 * @return int
 */
int pa2ew_server_pconnect_check( void )
{
	int            result = 0;
	double         time_now;
	CONNDESCRIP   *conn = PalertConns;

/* */
	if ( conn != NULL ) {
	/* */
		time_now = pa2ew_timenow_get();
		for ( int i = 0; i < MaxStationNum; i++, conn++ ) {
			if ( conn->sock != -1 ) {
				if ( (time_now - conn->last_act) > (double)PA2EW_IDLE_THRESHOLD ) {
					logit(
						"t", "palert2ew: Connection from %s idle over %d seconds, close connection!\n",
						conn->ip, PA2EW_IDLE_THRESHOLD
					);
					pa2ew_server_common_pconnect_close( conn, ThreadSets[i % ThreadsNumber].epoll_fd );
				}
				if ( conn->label.staptr )
					result++;
			}
		}
	}

	return result;
}

/**
 * @brief
 *
 * @param serial
 * @return CONNDESCRIP*
 */
CONNDESCRIP *pa2ew_server_pconnect_find( const uint16_t serial )
{
	return pa2ew_server_common_pconnect_find( PalertConns, MaxStationNum, serial );
}

/**
 * @brief
 *
 * @param max_stations
 * @param port
 * @param epoll
 * @param conn
 * @param accept_func
 * @return int
 */
int pa2ew_server_common_init(
	const int max_stations, const char *port, const int epoll, CONNDESCRIP **conn, int (*accept_func)( void )
) {
	int                result;
	struct epoll_event connevt;

/* Allocating Palerts' connection descriptors */
	if ( (*conn = calloc(max_stations, sizeof(CONNDESCRIP))) == NULL ) {
		logit("e", "palert2ew: Error allocating connection descriptors for port: %s!\n", port);
		return -1;
	}
	else {
		logit("o", "palert2ew: %d connection descriptors allocated for port: %s!\n", max_stations, port);
		for ( int i = 0; i < max_stations; i++ )
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

/**
 * @brief
 *
 * @param sock
 * @return CONNDESCRIP
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
	result.last_act = pa2ew_timenow_get();

	return result;
}

/**
 * @brief
 *
 * @param conn
 * @param conn_num
 * @param action
 * @param arg
 */
void pa2ew_server_common_pconnect_walk(
	const CONNDESCRIP *conn, const int conn_num, void (*action)(const void *, const int, void *), void *arg
) {
	if ( conn && conn_num ) {
		for ( int i = 0; i < conn_num; i++ )
			action( conn + i, i, arg );
	}

	return;
}

/**
 * @brief
 *
 * @param conn
 * @param conn_num
 * @param serial
 * @return CONNDESCRIP*
 */
CONNDESCRIP *pa2ew_server_common_pconnect_find( const CONNDESCRIP *conn, const int conn_num, const uint16_t serial )
{
	const CONNDESCRIP *result = NULL;
	_STAINFO          *staptr = NULL;

	if ( conn && conn_num ) {
		for ( int i = 0; i < conn_num; i++ ) {
			staptr = (_STAINFO *)conn[i].label.staptr;
			if ( staptr && serial == staptr->serial ) {
				result = conn + i;
				break;
			}
		}
	}

	return (CONNDESCRIP *)result;
}

/**
 * @brief Close the connect of the Palert.
 *
 * @param conn
 * @param epoll
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

/**
 * @brief Read the streaming data from each Palert and put it into queue.
 *
 * @param countindex
 * @param msec
 * @return int
 */
int pa2ew_server_proc( const int countindex, const int msec )
{
	int    nready;
	_Bool  need_update = 0;
	double time_now;
/* */
	int                  epoll  = ThreadSets[countindex].epoll_fd;
	LABELED_RECV_BUFFER *buffer = (LABELED_RECV_BUFFER *)ThreadSets[countindex].buffer;
	struct epoll_event  *evts   = ThreadSets[countindex].evts;

/* Wait the epoll for msec minisec */
	if ( (nready = epoll_wait(epoll, evts, PA2EW_MAX_PALERTS_PER_THREAD, msec)) ) {
		time_now = pa2ew_timenow_get();
	/* There is some incoming data from socket */
		for ( int i = 0; i < nready; i++ ) {
			if ( evts[i].events & EPOLLIN || evts[i].events & EPOLLRDHUP || evts[i].events & EPOLLERR ) {
				int          ret  = 0;
				CONNDESCRIP *conn = (CONNDESCRIP *)evts[i].data.ptr;
			/* */
				if ( (ret = recv(conn->sock, buffer->recv_buffer, PA2EW_RECV_BUFFER_LENGTH, 0)) <= 0 ) {
					if ( errno != EINTR ) {
						printf(
							"palert2ew: Palert IP:%s, read length:%d, errno:%d(%s), close connection!\n",
							conn->ip, ret, errno, strerror(errno)
						);
						pa2ew_server_common_pconnect_close( conn, epoll );
					}
				}
				else {
					if ( conn->label.staptr ) {
					/* Just send it to the main queue */
						buffer->label = conn->label;
						if (
							pa2ew_msgqueue_rawpacket(
								buffer, ret, PA2EW_GEN_MSG_LOGO_BY_SRC( PA2EW_MSG_SERVER_NORMAL )
							)
						) {
							if ( ++conn->sync_errors >= PA2EW_TCP_SYNC_ERR_LIMIT ) {
								logit(
									"et","palert2ew: Palert %d TCP connection sync error, close connection!\n",
									((_STAINFO *)conn->label.staptr)->serial
								);
								pa2ew_server_common_pconnect_close( conn, epoll );
							}
						}
						else {
							conn->sync_errors = 0;
						}
					}
					else if ( ret >= PALERT_M1_HEADER_LENGTH ) {
						if ( !pac_sync_check( buffer->recv_buffer ) ) {
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

/**
 * @brief Construct Palert listening connection socket.
 *
 * @param port
 * @return int
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

/**
 * @brief Accept the connection of Palerts then add it into connection descriptor and EpollFd.
 *
 * @return int
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

/**
 * @brief
 *
 * @param buffer
 * @param conn
 * @param epoll
 * @return int
 */
static int find_which_station( void *buffer, CONNDESCRIP *conn, int epoll )
{
	int       result   = 0;
	int       tzoffset = 0;
	uint16_t  serial   = pac_serial_get( ((LABELED_RECV_BUFFER *)buffer)->recv_buffer );
	_STAINFO *staptr   = pa2ew_list_find( serial );

/* */
	if ( !staptr ) {
	/* Not found in Palert table */
		printf("palert2ew: Serial(%d) not found in station list, maybe it's a new palert.\n", serial);
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
		conn->label.staptr   = staptr;
		conn->label.packmode = pac_mode_get( ((LABELED_RECV_BUFFER *)buffer)->recv_buffer );
	/* */
		if ( conn->label.packmode == PALERT_PKT_MODE1 || conn->label.packmode == PALERT_PKT_MODE2 ) {
			tzoffset = find_palert_tzoffset( (PALERT_M1_HEADER *)((LABELED_RECV_BUFFER *)buffer)->recv_buffer );
			staptr->timeshift = -(tzoffset * 3600);
		}
		else {
			staptr->timeshift = 0;
		}
	/* */
		printf("palert2ew: Palert %s in UTC%+.2d:00 with mode %02d packet now online.\n", staptr->sta, tzoffset, conn->label.packmode);
	}

	return result;
}

/**
 * @brief
 *
 * @param pah
 * @return int
 */
static int find_palert_tzoffset( const PALERT_M1_HEADER *pah )
{
	int     result = 0;
	time_t  ltime  = time(NULL);
	double  ptime  = pac_m1_systime_get( pah, 0 );
	double  offset = ptime - (double)ltime;

/* Turn the seconds to hours */
	offset = (offset + (offset > 0.0 ? 30.0 : -30.0)) / 3600.0;
	result = (int)(offset + (offset > 0.0 ? 0.1 : -0.1));

	return result;
}
