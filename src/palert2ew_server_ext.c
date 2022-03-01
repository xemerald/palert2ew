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
#include <sys/epoll.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <palert2ew.h>
#include <palert2ew_misc.h>
#include <palert2ew_ext.h>
#include <palert2ew_list.h>
#include <palert2ew_msg_queue.h>
#include <palert2ew_server.h>

/* Internal function prototype */
static int accept_palert_ext( void );
static int find_which_station( const uint16_t, CONNDESCRIP *, int );

/* Define global variables */
extern volatile int       AcceptEpoll;
static volatile int       AcceptSocketExt = -1;
static volatile int       MaxStationNum   = 0;
static PALERT_THREAD_SET *ThreadSetsExt   = NULL;
static CONNDESCRIP       *PalertConnsExt  = NULL;

/*
 * pa2ew_server_ext_init() - Initialize the independent Palert extension server &
 *                           return the needed threads number.
 */
int pa2ew_server_ext_init( const int max_stations, const char *port )
{
/* Check the constants */
	if ( !AcceptEpoll ) {
		logit("o", "palert2ew: Independent extension server initilaizing!\n");
		AcceptEpoll = epoll_create(1);
	}
/* */
	MaxStationNum = max_stations;
	ThreadSetsExt = calloc(1, sizeof(PALERT_THREAD_SET));
/* Construct the accept socket for retransmission */
	ThreadSetsExt->epoll_fd = epoll_create(MaxStationNum);
	ThreadSetsExt->buffer   = calloc(1, sizeof(LABELED_RECV_BUFFER));
	ThreadSetsExt->evts     = calloc(MaxStationNum, sizeof(struct epoll_event));
/* */
	AcceptSocketExt = pa2ew_server_common_init(
		MaxStationNum, port, AcceptEpoll, &PalertConnsExt, accept_palert_ext
	);
	if ( AcceptSocketExt <= 0 )
		return -1;

	return 1;
}

/*
 *  pa2ew_server_ext_end() - End process of Palert extension server.
 */
void pa2ew_server_ext_end( void )
{
	int i;

/* */
	logit("o", "palert2ew: Closing all the extension connections of Palerts!\n");
	if ( AcceptSocketExt != -1 )
		close(AcceptSocketExt);
/* Closing connections of Palerts */
	if ( PalertConnsExt != NULL ) {
		for ( i = 0; i < MaxStationNum; i++ )
			pa2ew_server_common_pconnect_close( (PalertConnsExt + i), ThreadSetsExt->epoll_fd );
		free(PalertConnsExt);
	}
/* Free epoll & readevts */
	if ( ThreadSetsExt != NULL ) {
		close(ThreadSetsExt->epoll_fd);
		free(ThreadSetsExt->buffer);
		free(ThreadSetsExt->evts);
		free(ThreadSetsExt);
	}

	return;
}

/*
 *
 */
void pa2ew_server_ext_pconnect_walk( void (*action)(const void *, const int, void *), void *arg )
{
	pa2ew_server_common_pconnect_walk( PalertConnsExt, MaxStationNum, action, arg );

	return;
}

/*
 * pa2ew_server_ext_pconnect_check() - Check connections of all Palerts.
 */
int pa2ew_server_ext_pconnect_check( void )
{
	int          i, result = 0;
	double       time_now;
	CONNDESCRIP *conn = PalertConnsExt;

/* */
	if ( conn != NULL ) {
		time_now = pa2ew_misc_timenow_get();
		for ( i = 0; i < MaxStationNum; i++, conn++ ) {
			if ( conn->sock != -1 ) {
				if ( (time_now - conn->last_act) > (double)PA2EW_IDLE_THRESHOLD ) {
					logit(
						"t", "palert2ew: Extension connection from %s idle over %d seconds, close connection!\n",
						conn->ip, PA2EW_IDLE_THRESHOLD
					);
					pa2ew_server_common_pconnect_close( conn, ThreadSetsExt->epoll_fd );
				}
				if ( conn->label.staptr )
					result++;
			}
		}
	}

	return result;
}

/*
 * pa2ew_server_ext_pconnect_find()
 */
CONNDESCRIP *pa2ew_server_ext_pconnect_find( const uint16_t serial )
{
	return pa2ew_server_common_pconnect_find( PalertConnsExt, MaxStationNum, serial );
}

/*
 *
 */
int pa2ew_server_ext_req_send( CONNDESCRIP *conn, const char *request, const int req_length )
{
	int result = -1;

/* */
	if ( conn != NULL && conn->sock > 0 ) {
		if ( send(conn->sock, request, req_length, 0) == req_length ) {
			result = 0;
		}
		else {
			logit(
				"et", "palert2ew: Error sending the extension request to serial(%d); close connection!\n",
				((_STAINFO *)conn->label.staptr)->serial
			);
			result = -2;
			pa2ew_server_common_pconnect_close( conn, ThreadSetsExt->epoll_fd );
		}
	}

	return result;
}

/*
 * pa2ew_server_ext_proc() - Read the retransmission data from each Palert and put it
 *                           into queue.
 */
int pa2ew_server_ext_proc( const int countindex, const int msec )
{
	int    i, nready;
	double time_now;
	_Bool  need_update = 0;
/* */
	int                  epoll  = ThreadSetsExt->epoll_fd;
	LABELED_RECV_BUFFER *buffer = (LABELED_RECV_BUFFER *)ThreadSetsExt->buffer;
	EXT_HEADER          *exth   = (EXT_HEADER *)&buffer->recv_buffer;
	struct epoll_event  *evts   = ThreadSetsExt->evts;
	const size_t         offset = buffer->recv_buffer - (uint8_t *)buffer;

/* Wait the epoll for msec minisec */
	if ( (nready = epoll_wait(epoll, evts, MaxStationNum, msec)) ) {
		time_now = pa2ew_misc_timenow_get();
	/* There is some incoming data from socket */
		for ( i = 0; i < nready; i++ ) {
			if ( evts[i].events & EPOLLIN || evts[i].events & EPOLLRDHUP || evts[i].events & EPOLLERR ) {
				int          ret  = 0;
				CONNDESCRIP *conn = (CONNDESCRIP *)evts[i].data.ptr;
			/* */
				if ( (ret = recv(conn->sock, &buffer->recv_buffer, PA2EW_EXT_MAX_PACKET_SIZE, 0)) <= 0 ) {
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
					/* */
						if ( exth->ext_type != PA2EW_EXT_TYPE_HEARTBEAT && ret == (int)exth->length ) {
							buffer->label = conn->label;
							if (
								pa2ew_msgqueue_enqueue(
									buffer, ret + offset, PA2EW_GEN_MSG_LOGO_BY_SRC( PA2EW_MSG_SERVER_EXT )
								)
							) {
								sleep_ew(100);
							}
						}
					}
					else if ( ret >= PA2EW_EXT_HEADER_SIZE ) {
					/* Find which Palert */
						need_update = find_which_station( exth->serial, conn, epoll );
					}
					else {
					/* Cannot received the hearbeat packet, close connection */
						printf(
							"palert2ew: Extension packet from Palert IP:%s is not heartbeat, close connection!\n",
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
	tmpconn = pa2ew_server_common_accept( AcceptSocketExt );
	if ( tmpconn.sock < 0 )
		return -1;
/* Find and save to an empty Palert connection */
	for ( i = 0, conn = PalertConnsExt; i < MaxStationNum; i++, conn++ ) {
		if ( conn->sock == -1 ) {
			*conn = tmpconn;
			acceptevt.data.ptr = conn;
			epoll_ctl(ThreadSetsExt->epoll_fd, EPOLL_CTL_ADD, conn->sock, &acceptevt);
			printf("palert2ew: New Palert extension connection from %s:%d.\n", conn->ip, conn->port);
			break;
		}
	}

	if ( i == MaxStationNum ) {
		printf(
			"palert2ew: Palert extension connection is full. Drop connection from %s:%d.\n",
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
static int find_which_station( const uint16_t serial, CONNDESCRIP *conn, int epoll )
{
	int       result = 0;
	_STAINFO *staptr = pa2ew_list_find( serial );

/* */
	if ( !staptr ) {
	/* Not found in Palert table */
		printf("palert2ew: Serial(%d) not found in station list, maybe it's a new palert.\n", serial);
	/* Drop the connection */
		result = -1;
		pa2ew_server_common_pconnect_close( conn, epoll );
	}
	else {
	/* Checking for existing connection with the same serial...*/
		CONNDESCRIP *_conn = pa2ew_server_common_pconnect_find( PalertConnsExt, MaxStationNum, serial );
		if ( _conn ) {
			pa2ew_server_common_pconnect_close( _conn, epoll );
		}
	/* */
		conn->label.staptr = staptr;
		staptr->ext_flag   = PA2EW_PALERT_EXT_ONLINE;
		printf("palert2ew: Palert %s extension now online.\n", staptr->sta);
	}

	return result;
}
