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
#include <sys/epoll.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <palert2ew.h>
#include <palert2ew_ext.h>
#include <palert2ew_list.h>
#include <palert2ew_msg_queue.h>
#include <palert2ew_server.h>

/* Internal function prototype */
static int accept_palert_ext( void );

/* Define global variables */
extern volatile int       MaxStationNum;
extern volatile int       AcceptEpoll;
static volatile int       AcceptSocketExt = -1;
static PALERT_THREAD_SET *ThreadSetsExt   = NULL;
static CONNDESCRIP       *PalertConnsExt  = NULL;
static MSG_LOGO           ExtLogo         = { 0 };


/*
 * pa2ew_server_ext_init() - Initialize the independent Palert extension server &
 *                           return the needed threads number.
 */
int pa2ew_server_ext_init( const char *port, const MSG_LOGO ext_logo )
{
/* Check the constants */
	if ( MaxStationNum && AcceptEpoll ) {
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
			return -2;
	/* */
		ExtLogo = ext_logo;
	}
	else {
		logit("e", "palert2ew: The raw Palert server has not been initiated yet!\n");
		return -1;
	}

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
			pa2ew_server_pconnect_close( (PalertConnsExt + i), ThreadSetsExt->epoll_fd );
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
 * pa2ew_server_ext_conn_check() - Check connections of all Palerts.
 */
int pa2ew_server_ext_conn_check( void )
{
	int          i, result = 0;
	time_t       time_now;
	CONNDESCRIP *conn = PalertConnsExt;

/* */
	if ( conn != NULL ) {
		for ( i = 0; i < MaxStationNum; i++, conn++ ) {
			if ( conn->sock != -1 ) {
				if ( (time(&time_now) - conn->last_act) >= PA2EW_IDLE_THRESHOLD ) {
					logit(
						"t", "palert2ew: Extension connection from %s idle over %d seconds, close connection!\n",
						conn->ip, PA2EW_IDLE_THRESHOLD
					);
					pa2ew_server_pconnect_close( conn, ThreadSetsExt->epoll_fd );
				}
				if ( conn->staptr )
					result++;
			}
		}
	}

	return result;
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
			if ( send(conn->sock, request, req_length, 0) == req_length ) {
				result = 0;
			}
			else {
				logit(
					"e", "palert2ew: Error sending the extension request to station %s; close connection!\n",
					staptr->sta
				);
				result = -2;
				pa2ew_server_pconnect_close( conn, ThreadSetsExt->epoll_fd );
			}
		}
	}

	return result;
}

/*
 *
 */
int pa2ew_server_ext_conn_reset( const _STAINFO *staptr )
{
	CONNDESCRIP *conn   = NULL;
	int          rtimes = PA2EW_EXT_CONNECT_RETRY_LIMIT;
	int          result = -1;

/* */
	if ( staptr != NULL ) {
		conn = (CONNDESCRIP *)staptr->ext_conn;
	/* */
		if ( conn != NULL && conn->sock > 0 )
			pa2ew_server_pconnect_close( conn, ThreadSetsExt->epoll_fd );
	/* */
		do {
			sleep_ew(1000);
			conn = (CONNDESCRIP *)staptr->ext_conn;
		/* */
			if ( conn != NULL && conn->sock == -1 ) {
				result = conn->sock;
				break;
			}
		} while ( --rtimes > 0 );
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
	time_t time_now;
	_Bool  need_update = 0;
/* */
	int                  epoll  = ThreadSetsExt->epoll_fd;
	LABELED_RECV_BUFFER *buffer = (LABELED_RECV_BUFFER *)ThreadSetsExt->buffer;
	EXT_HEADER          *exth   = (EXT_HEADER *)&buffer->recv_buffer;
	struct epoll_event  *evts   = ThreadSetsExt->evts;
	const size_t         offset = buffer->recv_buffer - (uint8_t *)buffer;

/* Wait the epoll for msec minisec */
	if ( (nready = epoll_wait(epoll, evts, MaxStationNum, msec)) ) {
		time(&time_now);
	/* There is some incoming data from socket */
		for ( i = 0; i < nready; i++ ) {
			if ( evts[i].events & EPOLLIN || evts[i].events & EPOLLRDHUP || evts[i].events & EPOLLERR ) {
				int          ret  = 0;
				CONNDESCRIP *conn = (CONNDESCRIP *)evts[i].data.ptr;
			/* */
				conn->last_act = time_now;
				if ( (ret = recv(conn->sock, &buffer->recv_buffer, PA2EW_EXT_MAX_PACKET_SIZE, 0)) <= 0 ) {
					printf(
						"palert2ew: Palert IP:%s, read length:%d, errno:%d(%s), close connection!\n",
						conn->ip, ret, errno, strerror(errno)
					);
					pa2ew_server_pconnect_close( conn, epoll );
				}
				else {
					if ( conn->staptr ) {
						if ( exth->ext_type != PA2EW_EXT_TYPE_HEARTBEAT && ret == (int)exth->length ) {
							buffer->sptr = conn->staptr;
							if ( pa2ew_msgqueue_enqueue( buffer, ret + offset, ExtLogo ) )
								sleep_ew(100);
						}
					}
					else if ( ret >= PA2EW_EXT_HEADER_SIZE ) {
					/* Find which Palert */
						uint16_t serial = exth->serial;
						if ( (conn->staptr = pa2ew_list_find( serial )) == NULL ) {
						/* Not found in Palert table */
							printf("palert2ew: %d not found in station list, maybe it's a new palert.\n", serial);
						/* Drop the connection */
							need_update = 1;
							pa2ew_server_pconnect_close( conn, epoll );
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
						pa2ew_server_pconnect_close( conn, epoll );
					}
				}
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