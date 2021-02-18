/*
 *
 */
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
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
#include <palert2ew_list.h>
#include <palert2ew_msg_queue.h>

/* */
#define LISTENQ  128

/* Connection descriptors struct */
typedef struct {
	int       sock;
	int       port;
	char      ip[INET6_ADDRSTRLEN];
	uint8_t   sync_errors;
	time_t    last_act;
	_STAINFO *staptr;
} CONNDESCRIP;

/* */
typedef struct {
	int                 epoll_fd;
	uint8_t            *buffer;
	struct epoll_event *evts;
} PALERT_THREAD_SET;

/* Internal function prototype */
static int  proc_server_normal( const int, const int );
static int  proc_server_ext( const int, const int );
static int  init_palert_server_common( const int, const char *, const int, CONNDESCRIP **, int (*)( void ) );
static int  construct_listen_sock( const char * );
static int  accept_palert_normal( void );
static int  accept_palert_ext( void );
static CONNDESCRIP real_accept_connection( const int );
static int  eval_threadnum( int );
static void close_palert_connect( CONNDESCRIP *, unsigned int );

/* Define global variables */
static volatile int       AcceptEpoll        = 0;
static volatile int       AcceptSocket       = -1;
static volatile int       AcceptSocketRt     = -1;
static volatile int       MaxStationNum      = 0;
static volatile int       ThreadsNumber      = 0;
static PALERT_THREAD_SET *PalertThreadSets   = NULL;
static PALERT_THREAD_SET *PalertExtThreadSet = NULL;
static CONNDESCRIP       *PalertConns        = NULL;
static CONNDESCRIP       *PalertConnsExt     = NULL;
static MSG_LOGO           ExtLogo            = { 0 };

/* Macro */
#define RESET_CONNDESCRIP(CONN) \
	__extension__({ \
		memset((CONN), 0, sizeof(CONNDESCRIP)); \
		(CONN)->sock   = -1; \
		(CONN)->staptr = NULL; \
	})

/*
 * pa2ew_server_init() - Initialize the independent Palert server &
 *                       return the needed threads number.
 */
int pa2ew_server_init( const int max_stations, const char *port, const char *rt_port, const MSG_LOGO ext_logo )
{
	int i;

/* Setup constants */
	AcceptEpoll      = epoll_create(2);
	MaxStationNum    = max_stations;
	ThreadsNumber    = eval_threadnum( max_stations );
	PalertThreadSets = calloc(ThreadsNumber, sizeof(PALERT_THREAD_SET));
/* Create epoll sets */
	for ( i = 0; i < ThreadsNumber; i++ ) {
		PalertThreadSets[i].epoll_fd = epoll_create(PA2EW_MAX_PALERTS_PER_THREAD);
		PalertThreadSets[i].buffer   = calloc(PA2EW_RECV_BUFFER_LENGTH, 1);
		PalertThreadSets[i].evts     = calloc(PA2EW_MAX_PALERTS_PER_THREAD, sizeof(struct epoll_event));
	}
/* Construct the accept socket for normal stream */
	if ( port != NULL && strlen(port) ) {
		AcceptSocket = init_palert_server_common(
			max_stations, port, AcceptEpoll, &PalertConns, accept_palert_normal
		);
		if ( AcceptSocket <= 0 )
			return -1;
	}
/* Construct the accept socket for retransmission */
	if ( rt_port != NULL && strlen(rt_port) ) {
		PalertExtThreadSet = calloc(1, sizeof(PALERT_THREAD_SET));
		PalertExtThreadSet->epoll_fd = epoll_create(max_stations);
		PalertExtThreadSet->buffer   = calloc(PA2EW_EXT_MAX_PACKET_SIZE, 1);
		PalertExtThreadSet->evts     = calloc(max_stations, sizeof(struct epoll_event));
	/* */
		AcceptSocketRt = init_palert_server_common(
			max_stations, rt_port, AcceptEpoll, &PalertConnsExt, accept_palert_ext
		);
		if ( AcceptSocketRt <= 0 )
			return -2;
	/* */
		ExtLogo = ext_logo;
	/* */
		return ThreadsNumber + 1;
	}

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
	if ( AcceptSocket != -1 ) close(AcceptSocket);
	if ( AcceptSocketRt != -1 ) close(AcceptSocketRt);
	close(AcceptEpoll);
/* Closing connections of Palerts */
	if ( PalertConns != NULL ) {
		for ( i = 0; i < MaxStationNum; i++ )
			close_palert_connect( (PalertConns + i), i % 2 );
		free(PalertConns);
	}
	if ( PalertConnsExt != NULL ) {
		for ( i = 0; i < MaxStationNum; i++ )
			close_palert_connect( (PalertConnsExt + i), 2 );
		free(PalertConnsExt);
	}
/* Free epoll & readevts */
	if ( PalertThreadSets != NULL ) {
		for ( i = 0; i < ThreadsNumber; i++ ) {
			close(PalertThreadSets[i].epoll_fd);
			free(PalertThreadSets[i].buffer);
			free(PalertThreadSets[i].evts);
		}
		free(PalertThreadSets);
	}
	if ( PalertExtThreadSet != NULL ) {
		close(PalertExtThreadSet->epoll_fd);
		free(PalertExtThreadSet->buffer);
		free(PalertExtThreadSet->evts);
		free(PalertExtThreadSet);
	}

	return;
}

/*
 * pa2ew_server_stream() - Read the streaming data from each Palert and put it
 *                         into queue.
 */
int pa2ew_server_proc( const int countindex, const int msec )
{
	if ( countindex < ThreadsNumber )
		return proc_server_normal( countindex, msec );
	else
		return proc_server_ext( countindex, msec );
}

/*
 *
 */
int pa2ew_server_ext_req_send( const _STAINFO *staptr, const char *request, const int req_length )
{
	int result = -1;
	CONNDESCRIP *conn = NULL;

/* */
	if ( staptr != NULL ) {
		conn = (CONNDESCRIP *)staptr->ext_conn;
		if ( conn != NULL && conn->sock > 0 ) {
			if ( send(conn->sock, request, req_length, 0) == req_length )
				result = 0;
			else
				result = -2;
		}
	}

	return result;
}

/*
 * pa2ew_server_conn_check() - Check connections of all Palerts.
 */
int pa2ew_server_conn_check( void )
{
	int          i, result = 0;
	time_t       time_now;
	CONNDESCRIP *conn = PalertConns;

/* */
	for ( i = 0; i < MaxStationNum; i++, conn++ ) {
		if ( conn->sock != -1 ) {
			if ( (time(&time_now) - conn->last_act) >= 120 ) {
				logit("t", "palert2ew: Connection: %s idle over two minutes, close connection!\n", conn->ip);
				close_palert_connect( conn, i % ThreadsNumber );
			}
			if ( conn->staptr )
				result++;
		}
	}
/* */
	if ( PalertConnsExt != NULL ) {
		conn = PalertConnsExt;
		for ( i = 0; i < MaxStationNum; i++, conn++ ) {
			if ( conn->sock != -1 ) {
				if ( (time(&time_now) - conn->last_act) >= 120 ) {
					logit(
						"t", "palert2ew: Extension connection: %s idle over two minutes, close connection!\n",
						conn->ip
					);
					close_palert_connect( conn, ThreadsNumber );
				}
			}
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
 * proc_server_normal() - Read the streaming data from each Palert and put it
 *                        into queue.
 */
static int proc_server_normal( const int countindex, const int msec )
{
	int    i, nready;
	time_t time_now;
	_Bool  need_update = 0;
/* */
	int                 epoll  = PalertThreadSets[countindex].epoll_fd;
	uint8_t            *buffer = PalertThreadSets[countindex].buffer;
	struct epoll_event *evts   = PalertThreadSets[countindex].evts;

/* Wait the epoll for msec minisec */
	if ( (nready = epoll_wait(epoll, evts, PA2EW_MAX_PALERTS_PER_THREAD, msec)) ) {
		time(&time_now);
	/* There is some incoming data from socket */
		for ( i = 0; i < nready; i++ ) {
			if ( evts[i].events & EPOLLIN || evts[i].events & EPOLLRDHUP || evts[i].events & EPOLLERR ) {
				int          ret  = 0;
				CONNDESCRIP *conn = (CONNDESCRIP *)evts[i].data.ptr;
			/* */
				conn->last_act = time_now;
				if ( (ret = recv(conn->sock, buffer, PA2EW_RECV_BUFFER_LENGTH, 0)) <= 0 ) {
					printf(
						"palert2ew: Palert IP:%s, read length:%d, errno:%d(%s), close connection!\n",
						conn->ip, ret, errno, strerror(errno)
					);
					close_palert_connect( conn, countindex );
				}
				else {
					if ( conn->staptr ) {
					/* Process message */
						if ( (ret = pa2ew_msgqueue_rawpacket( conn->staptr, buffer, ret )) ) {
							if ( ret == 1 ) {
								if ( ++(conn->sync_errors) >= 10 ) {
									logit(
										"e","palert2ew: Palert %s TCP connection sync error, close connection!\n",
										conn->staptr->sta
									);
									close_palert_connect( conn, countindex );
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
						PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)buffer;
						if ( !PALERTMODE1_HEADER_CHECK_SYNC( pah ) ) {
							printf("palert2ew: Palert IP:%s sync failure, close connection!\n", conn->ip);
							close_palert_connect( conn, countindex );
						}
						else {
						/* Find which Palert */
							uint16_t serial = PALERTMODE1_HEADER_GET_SERIAL( pah );
							if ( (conn->staptr = pa2ew_list_find( serial )) == NULL ) {
							/* Not found in Palert table */
								printf("palert2ew: %d not found in station list, maybe it's a new palert.\n", serial);
							/* Drop the connection */
								need_update = 1;
								close_palert_connect( conn, countindex );
							}
							else {
								conn->staptr->normal_conn = conn;
								printf("palert2ew: Palert %s now online.\n", conn->staptr->sta);
							}
						}
					}
					else {
					/* Receive data not enough, close connection */
						printf(
							"palert2ew: Palert IP:%s send data not enough to check, close connection!\n",
							conn->ip
						);
						close_palert_connect( conn, countindex );
					}
				}
			}
		}
	}
/* */
	return need_update ? PA2EW_RECV_NEED_UPDATE : PA2EW_RECV_NORMAL;
}

/*
 * proc_server_ext() - Read the retransmission data from each Palert and put it
 *                     into queue.
 */
static int proc_server_ext( const int countindex, const int msec )
{
	int    i, nready;
	time_t time_now;
	_Bool  need_update = 0;
/* */
	int                 readepoll = PalertExtThreadSet->epoll_fd;
	EXT_HEADER         *readptr   = (EXT_HEADER *)PalertExtThreadSet->buffer;
	struct epoll_event *readevts  = PalertExtThreadSet->evts;

/* Wait the epoll for msec minisec */
	if ( (nready = epoll_wait(readepoll, readevts, MaxStationNum, msec)) ) {
		time(&time_now);
	/* There is some incoming data from socket */
		for ( i = 0; i < nready; i++ ) {
			if ( readevts[i].events & EPOLLIN || readevts[i].events & EPOLLRDHUP || readevts[i].events & EPOLLERR ) {
				int          ret  = 0;
				CONNDESCRIP *conn = (CONNDESCRIP *)readevts[i].data.ptr;
			/* */
				conn->last_act = time_now;
				if ( (ret = recv(conn->sock, readptr, PA2EW_EXT_MAX_PACKET_SIZE, 0)) <= 0 ) {
					printf(
						"palert2ew: Palert IP:%s, read length:%d, errno:%d(%s), close connection!\n",
						conn->ip, ret, errno, strerror(errno)
					);
					close_palert_connect( conn, countindex );
				}
				else {
					if ( conn->staptr ) {
						if ( readptr->ext_type != PA2EW_EXT_TYPE_HEARTBEAT && ret == (int)readptr->length ) {
							if ( pa2ew_msgqueue_enqueue( readptr, readptr->length, ExtLogo ) )
								sleep_ew(100);
						}
					}
					else if ( ret >= PA2EW_EXT_HEADER_SIZE ) {
					/* Find which Palert */
						uint16_t serial = readptr->serial;
						if ( (conn->staptr = pa2ew_list_find( serial )) == NULL ) {
						/* Not found in Palert table */
							printf("palert2ew: %d not found in station list, maybe it's a new palert.\n", serial);
						/* Drop the connection */
							need_update = 1;
							close_palert_connect( conn, countindex );
						}
						else {
							conn->staptr->ext_conn = conn;
							printf("palert2ew: Palert %s extension now online.\n", conn->staptr->sta);
						}
					}
					else {
					/* Cannot received the hearbeat packet, close connection */
						printf(
							"palert2ew: Extension packet from Palert IP:%s is not heartbeat, close connection!\n",
							conn->ip
						);
						close_palert_connect( conn, countindex );
					}
				}
			}
		}
	}
/* */
	return need_update ? PA2EW_RECV_NEED_UPDATE : PA2EW_RECV_NORMAL;
}

/*
 *
 */
static int init_palert_server_common(
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
 * accept_palert_normal() - Accept the connection of Palerts then add it
 *                          into connection descriptor and EpollFd.
 */
static int accept_palert_normal( void )
{
	int                i;
	CONNDESCRIP       *conn = NULL;
	CONNDESCRIP        tmpconn;
	struct epoll_event acceptevt;

/* */
	acceptevt.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
	acceptevt.data.ptr = NULL;
/* */
	tmpconn = real_accept_connection( AcceptSocket );
	if ( tmpconn.sock < 0 )
		return -1;
/* Find and save to an empty Palert connection */
	for ( i = 0, conn = PalertConns; i < MaxStationNum; i++, conn++ ) {
		if ( conn->sock == -1 ) {
			*conn = tmpconn;
			acceptevt.data.ptr = conn;
			epoll_ctl(PalertThreadSets[i % ThreadsNumber].epoll_fd, EPOLL_CTL_ADD, conn->sock, &acceptevt);
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
 * accept_palert_ext() - Accept the connection of Palerts then add it
 *                       into connection descriptor and EpollFd.
 */
static int accept_palert_ext( void )
{
	int                i;
	CONNDESCRIP       *conn = NULL;
	CONNDESCRIP        tmpconn;
	struct epoll_event acceptevt;

/* */
	acceptevt.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
	acceptevt.data.ptr = NULL;
/* */
	tmpconn = real_accept_connection( AcceptSocketRt );
	if ( tmpconn.sock < 0 )
		return -1;
/* Find and save to an empty Palert connection */
	for ( i = 0, conn = PalertConnsExt; i < MaxStationNum; i++, conn++ ) {
		if ( conn->sock == -1 ) {
			*conn = tmpconn;
			acceptevt.data.ptr = conn;
			epoll_ctl(PalertExtThreadSet->epoll_fd, EPOLL_CTL_ADD, conn->sock, &acceptevt);
			printf("palert2ew: New Palert retransmission connection from %s:%d.\n", conn->ip, conn->port);
			break;
		}
	}

	if ( i == MaxStationNum ) {
		printf(
			"palert2ew: Palert retransmission connection is full. Drop connection from %s:%d.\n",
			tmpconn.ip, tmpconn.port
		);
		close(tmpconn.sock);
		return -2;
	}

	return 0;
}

/*
 *
 */
static CONNDESCRIP real_accept_connection( const int sock )
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
	time(&result.last_act);

	return result;
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
		if ( (int)index < ThreadsNumber )
			epoll_ctl(PalertThreadSets[index].epoll_fd, EPOLL_CTL_DEL, conn->sock, &tmpev);
		else if ( PalertExtThreadSet != NULL )
			epoll_ctl(PalertExtThreadSet->epoll_fd, EPOLL_CTL_DEL, conn->sock, &tmpev);
		close(conn->sock);
	/* */
		RESET_CONNDESCRIP( conn );
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
