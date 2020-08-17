/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
/* Network related header include */
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <palert2ew.h>
#include <palert2ew_list.h>
#include <palert2ew_msg_queue.h>

/* */
#define LISTENQ  128

/* Connection descriptors struct */
typedef struct {
	int       sock;
	char      ip[INET6_ADDRSTRLEN];
	uint8_t   sync_errors;
	uint8_t   is_palert;
	time_t    last_act;
	_STAINFO *staptr;
} CONNDESCRIP;

/* */
typedef struct {
	int                epoll_fd;
	uint8_t            buffer[PREPACKET_LENGTH];
	struct epoll_event evts[PA2EW_MAX_PALERTS_PER_THREAD];
} PALERT_THREAD_SET;

/* Internal function prototype */
static int  construct_listen_sock( const char * );
static int  real_accept_palert( void );
static int  eval_threadnum( int );
static void close_palert_connect( CONNDESCRIP *, unsigned int );

/* Define global variables */
static volatile int       AcceptEpoll      = 0;
static volatile int       AcceptSocket     = -1;
static volatile int       MaxStationNum    = 0;
static volatile int       ThreadsNumber    = 0;
static PALERT_THREAD_SET *PalertThreadSets = NULL;
static CONNDESCRIP       *PalertConns      = NULL;

/*
 * pa2ew_server_init() - Initialize the independent Palert server &
 *                       return the needed threads number.
 */
int pa2ew_server_init( const int max_stations )
{
	int i;
	struct epoll_event connevt;

/* Setup constants */
	AcceptEpoll      = epoll_create(1);
	MaxStationNum    = max_stations;
	ThreadsNumber    = eval_threadnum( max_stations );
	PalertThreadSets = calloc(ThreadsNumber, sizeof(PALERT_THREAD_SET));
/* Create epoll sets */
	for ( i = 0; i < ThreadsNumber; i++ )
		PalertThreadSets[i].epoll_fd = epoll_create(PA2EW_MAX_PALERTS_PER_THREAD);
/* Allocating Palerts' connection descriptors */
	if ( (PalertConns = calloc(max_stations, sizeof(CONNDESCRIP))) == NULL ) {
		logit("e", "palert2ew: Error allocating connection descriptors!\n");
		return -1;
	}
	else {
		logit("", "palert2ew: %d connection descriptors allocated!\n", max_stations);
		for ( i = 0; i < max_stations; i++ )
			PalertConns[i].sock = -1;
	}
/* Construct the accept socket */
	if ( (AcceptSocket = construct_listen_sock( PA2EW_PALERT_PORT )) == -1 )
		return -2;
/* */
	connevt.events   = EPOLLIN | EPOLLERR;
	connevt.data.ptr = real_accept_palert;
	epoll_ctl(AcceptEpoll, EPOLL_CTL_ADD, AcceptSocket, &connevt);

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
	close(AcceptSocket);
	close(AcceptEpoll);
/* Closing connections of Palerts */
	for ( i = 0; i < MaxStationNum; i++ ) close_palert_connect( (PalertConns + i), i % 2 );
	free(PalertConns);
/* Free epoll & readevts */
	for ( i = 0; i < ThreadsNumber; i++ )
		close(PalertThreadSets[i].epoll_fd);

	return;
}

/*
 * pa2ew_server_stream() - Read the streaming data from each Palert and put it
 *                         into queue.
 */
int pa2ew_server_stream( const int countindex, const int msec )
{
	int    i, nready;
	time_t time_now;
	_Bool  need_update = 0;
/* */
	int                 readepoll = PalertThreadSets[countindex].epoll_fd;
	PREPACKET          *readptr   = (PREPACKET *)PalertThreadSets[countindex].buffer;
	struct epoll_event *readevts  = PalertThreadSets[countindex].evts;

/* Wait the epoll for msec minisec */
	if ( (nready = epoll_wait(readepoll, readevts, PA2EW_MAX_PALERTS_PER_THREAD, msec)) ) {
		time(&time_now);
	/* There is some incoming data from socket */
		for ( i = 0; i < nready; i++ ) {
			if ( readevts[i].events & EPOLLIN || readevts[i].events & EPOLLRDHUP || readevts[i].events & EPOLLERR ) {
				int          ret  = 0;
				CONNDESCRIP *conn = (CONNDESCRIP *)readevts[i].data.ptr;
			/* */
				conn->last_act = time_now;
				if ( (ret = recv(conn->sock, readptr->data, DATA_BUFFER_LENGTH, 0)) <= 0 ) {
					printf(
						"palert2ew: Palert IP:%s, read length:%d, errno:%d(%s), close connection!\n",
						conn->ip, ret, errno, strerror(errno)
					);
					close_palert_connect(conn, countindex);
					continue;
				}
				else {
					if ( conn->is_palert == 1 ) {
					/* Process message */
						readptr->len = ret;
						if ( (ret = pa2ew_msgqueue_prequeue( conn->staptr, readptr )) != 0 ) {
							if ( ret == 1 ) {
								if ( ++(conn->sync_errors) >= 10 ) {
									conn->sync_errors = 0;
									logit("e","palert2ew: Palert %s TCP connection sync error, close connection!\n", conn->staptr->sta);
									close_palert_connect(conn, countindex);
								}
							}
							else {
								sleep_ew(100);
							}
						}
						else {
							conn->sync_errors = 0;
						}
					}
					else if ( ret >= 200 ) {
						PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)(readptr->data);
						if( !PALERTMODE1_HEADER_CHECK_SYNC( pah ) ) {
							printf("palert2ew: Palert IP:%s sync failure, close connection!\n", conn->ip);
							close_palert_connect(conn, countindex);
						}
						else {
						/* Find which Palert */
							uint16_t  serial = PALERTMODE1_HEADER_GET_SERIAL( pah );
							_STAINFO *staptr = palert2ew_list_find( serial );
							if ( staptr == NULL ) {
							/* Not found in Palert table */
								printf(
									"palert2ew: %d not found in station list, maybe it's a new palert.\n", serial
								);
							/* Drop the connection */
								need_update = 1;
								close_palert_connect(conn, countindex);
							}
							else {
								printf("palert2ew: Palert %s now online.\n", staptr->sta);
								conn->is_palert = 1;
								conn->staptr    = staptr;
							}
						}
					}
				/* Receive data not enough, close connection */
					else {
						printf(
							"palert2ew: Palert IP:%s send data not enough to check, close connection!\n",
							conn->ip
						);
						close_palert_connect(conn, countindex);
					}
				}
			}
		}
	}
/* */
	if ( need_update )
		return -1;
	else
		return 0;
}

/*
 * pa2ew_server_conn_check() - Check connections of all Palerts.
 */
int pa2ew_server_conn_check( void )
{
	int          i, result = 0;
	time_t       time_now;
	CONNDESCRIP *conn    = NULL;

/* */
	for ( i = 0; i < MaxStationNum; i++ ) {
		conn = PalertConns + i;
		if ( conn->sock != -1 ) {
			if ( (time(&time_now) - conn->last_act) >= 120 ) {
				logit("t", "palert2ew: Connection: %s idle over two minutes, close connection!\n", conn->ip);
				close_palert_connect( conn, i % ThreadsNumber );
			}
			if ( conn->is_palert )
				result++;
		}
	}

	return result;
}

/*
 *
 */
int pa2ew_server_palert_accept( const int msec )
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
			logit("o", "palert2ew: Listen Palert connection socket ready!\n");
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
 * real_accept_palert() - Accept the connection of Palerts then add it
 *                        into connection descriptor and EpollFd.
 */
static int real_accept_palert( void )
{
	int  i;
	int  cliport;
	int  connsd;
	char connip[INET6_ADDRSTRLEN];

	struct epoll_event      acceptevt;
	struct sockaddr_storage cliaddr;
	struct sockaddr_in     *cli4_ptr = (struct sockaddr_in *)&cliaddr;
	struct sockaddr_in6    *cli6_ptr = (struct sockaddr_in6 *)&cliaddr;
	unsigned int            clilen   = sizeof(cliaddr);
	CONNDESCRIP            *conn     = NULL;

/* */
	acceptevt.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
	acceptevt.data.ptr = NULL;
	if ( (connsd = accept(AcceptSocket, (struct sockaddr *)&cliaddr, &clilen)) == -1 ) {
		printf("palert2ew: Accepted new Palert's connection error!\n");
		return -1;
	}

	switch ( cliaddr.ss_family ) {
	case AF_INET:
		inet_ntop(cliaddr.ss_family, &cli4_ptr->sin_addr, connip, sizeof(connip));
		cliport = (int)cli4_ptr->sin_port;
		break;
	case AF_INET6:
		inet_ntop(cliaddr.ss_family, &cli6_ptr->sin6_addr, connip, sizeof(connip));
		cliport = (int)cli6_ptr->sin6_port;
		break;
	default:
		printf("palert2ew: Accept socket internet type unknown, drop it!\n");
		return -1;
		break;
	}

/* Find and save to an empty Palert connection */
	for ( i = 0; i < MaxStationNum; i++ ) {
		conn = PalertConns + i;
		if ( conn->sock == -1 ) {
			int tmp = i % ThreadsNumber;
			conn->sock = connsd;
			conn->sync_errors = 0;
			strcpy(conn->ip, connip);
			time(&conn->last_act);
			acceptevt.data.ptr = conn;
			epoll_ctl(PalertThreadSets[tmp].epoll_fd, EPOLL_CTL_ADD, conn->sock, &acceptevt);
			printf("palert2ew: New Palert connection from %s:%d.\n", connip, cliport);
			break;
		}
	}

	if ( i == MaxStationNum ) {
		printf("palert2ew: Palert connection is full. Drop connection from %s:%d.\n", connip, cliport);
		close(connsd);
		return -2;
	}

	return 0;
}

/*
 * close_palert_connect() - Close the connect of the Palert.
 */
static void close_palert_connect( CONNDESCRIP *conn, unsigned int index )
{
	if ( conn->sock != -1 ) {
		struct epoll_event tmpev;
		tmpev.events   = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
		tmpev.data.ptr = conn;
		epoll_ctl(PalertThreadSets[index].epoll_fd, EPOLL_CTL_DEL, conn->sock, &tmpev);
		close(conn->sock);
		memset(conn, 0, sizeof(CONNDESCRIP));
		conn->sock = -1;
	}
	return;
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
