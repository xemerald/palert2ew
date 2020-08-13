/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* Network related header include */
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Earthworm environment header include */
#include <earthworm.h>

/* Local header include */
#include <palert.h>
#include <palert2ew.h>
#include <palert2ew_list.h>
#include <palert2ew_socket.h>
#include <palert2ew_msg_queue.h>

/* Listening Palert port */
#define PALERT_PORT "502"

/* Connection descriptors struct */
typedef struct {
	int       sd;
	char      ip[INET6_ADDRSTRLEN];
	uint8_t   syncErrCnt;
	uint8_t   isPalert;
	time_t    lastAct;
	_STAINFO *staptr;
} CONNDESCRIP;

/* Internal function prototype */
static int  AcceptPalert( void );
static void ClosePalertConn( CONNDESCRIP *, unsigned int );

/* Define global variables */
static volatile int AcceptSocket = -1;
static volatile int EpollFd[2];

static uint32_t MaxEpollPalert = 0;
static uint32_t _MaxStationNum = 0;

static CONNDESCRIP        *PalertConn  = NULL;
static uint8_t            *Buffer[2] = { NULL };
static struct epoll_event *ReadEvts[2] = { NULL };


/**********************************************************************
 *  PalertServerInit( ) -- Initialize the independent Palert server.  *
 *  Arguments:                                                        *
 *    maxStationNum = Maximum station number.                         *
 *  Returns:                                                          *
 *     0 = Normal, success.                                           *
 *    -1 = Error, allocating memory failed.                           *
 *    -2 = Error, construct socket failed.                            *
 **********************************************************************/
int PalertServerInit( const unsigned int maxStationNum )
{
	uint32_t i;

	struct epoll_event connevt;

/* Setup constants */
	_MaxStationNum = maxStationNum;
	MaxEpollPalert = maxStationNum/2;

/* Create epoll sets */
	for ( i=0; i<2; i++ ) {
		EpollFd[i] = epoll_create(MaxEpollPalert + i);
		if ( (ReadEvts[i] = calloc(MaxEpollPalert + i, sizeof(struct epoll_event))) == NULL ) {
			logit( "e", "palert2ew: Error allocating epoll events!\n" );
			return -1;
		}
		if ( (Buffer[i] = calloc(1, sizeof(PREPACKET))) == NULL ) {
			logit( "e", "palert2ew: Error allocating reading buffer!\n" );
			return -1;
		}
	}

/* Allocating Palerts' connection descriptors */
	if ( (PalertConn = calloc(maxStationNum, sizeof(CONNDESCRIP))) == NULL ) {
		logit( "e", "palert2ew: Error allocating connection descriptors!\n" );
		return -1;
	}
	else logit( "", "palert2ew: %d connection descriptors allocated!\n", maxStationNum );

	for ( i=0; i<maxStationNum; i++ ) (PalertConn + i)->sd = -1;

/* Construct the accept socket */
	if ( (AcceptSocket = ConstructSocket(NULL, PALERT_PORT)) == -1 ) {
		return -2;
	}

	connevt.events = EPOLLIN | EPOLLERR;
	connevt.data.fd = AcceptSocket;
	epoll_ctl(EpollFd[1], EPOLL_CTL_ADD, AcceptSocket, &connevt);

	return 0;
}

/**********************************************************************
 *  ReadPalertsData( ) -- Read the data from each Palert and put it   *
 *                        into queue.                                 *
 *  Arguments:                                                        *
 *    countindex = Thread number, there should be only 0 & 1.         *
 *    msec       = Waiting time in minisecond.                        *
 *  Returns:                                                          *
 *     0 = Normal.                                                    *
 **********************************************************************/
int ReadPalertsData( const unsigned short countindex, const unsigned int msec )
{
	int ret;

	uint32_t j;
	uint32_t nready;

	uint8_t      needUpdate = 0;
	time_t       timenow;
	_STAINFO    *staptr = NULL;
	CONNDESCRIP *tmpconn_p = NULL;

	PREPACKET          *readptr  = (PREPACKET *)Buffer[countindex];
	struct epoll_event *readevts = ReadEvts[countindex];


/* Wait the epoll for 1 sec */
	if ( (nready = epoll_wait(EpollFd[countindex], readevts, MaxEpollPalert + countindex, msec)) ) {
		time(&timenow);

	/* There is some incoming data from socket */
		for ( j=0; j<nready; j++ ) {
			if( readevts[j].events & EPOLLIN ||
				readevts[j].events & EPOLLRDHUP ||
				readevts[j].events & EPOLLERR )
			{
			/* Check if the socket is the accept socket */
				if ( countindex && readevts[j].data.fd == AcceptSocket ) {
					AcceptPalert();
					continue;
				}
				else {
					tmpconn_p = readevts[j].data.ptr;
					tmpconn_p->lastAct = timenow;

					if ( (ret = recv(tmpconn_p->sd, readptr->data, DATA_BUFFER_LENGTH, 0)) <= 0 ) {
						printf("palert2ew: Palert IP:%s, read length:%d, errno:%d(%s), close connection!\n", tmpconn_p->ip, ret, errno, strerror(errno) );
						ClosePalertConn(tmpconn_p, countindex);
						continue;
					}
					else {
						if ( tmpconn_p->isPalert == 1 ) {
						/* Process message */
							readptr->len = ret;
							if ( (ret = MsgEnqueue( readptr, tmpconn_p->staptr )) != 0 ) {
								if ( ret == -1 ) {
									if ( ++(tmpconn_p->syncErrCnt) >= 10 ) {
										tmpconn_p->syncErrCnt = 0;
										logit("e","palert2ew: Palert %s TCP connection sync error, close connection!\n", tmpconn_p->staptr->sta_c);
										ClosePalertConn(tmpconn_p, countindex);
									}
								}
								else {
									sleep_ew(200);
									continue;
								}
							}
							else tmpconn_p->syncErrCnt = 0;
						}
						else if ( ret >= 200 ) {
							PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)(readptr->data);

							if( !PALERTMODE1_HEADER_CHECK_SYNC( pah ) ) {
								printf("palert2ew: Palert IP:%s sync failure, close connection!\n", tmpconn_p->ip);
								ClosePalertConn(tmpconn_p, countindex);
							}
							else {
								uint16_t serial = PALERTMODE1_HEADER_GET_SERIAL( pah );

							/* Find which Palert */
								staptr = palert2ew_list_find( serial );

								if ( staptr == NULL ) {
								/* Not found in Palert table */
									printf("palert2ew: %d not found in station table, maybe it's a new palert.\n", serial);
								/* Drop the connection */
									needUpdate = 1;
									ClosePalertConn(tmpconn_p, countindex);
									continue;
								}

								printf("palert2ew: Palert %s now online.\n", staptr->sta_c);

								tmpconn_p->isPalert = 1;
								tmpconn_p->staptr = staptr;
							}
						}
					/* Receive data not enough, close connection */
						else {
							printf("palert2ew: Palert IP:%s send data not enough to check, close connection!\n", tmpconn_p->ip);
							ClosePalertConn(tmpconn_p, countindex);
						}
					}
				}
			}
		}
	}

	if ( needUpdate ) return -1;
	else return 0;
}


/**********************************************************************
 *  AcceptPalert( ) -- Accept the connection of Palerts then add it   *
 *                     into connection descriptor and EpollFd.        *
 *  Arguments:                                                        *
 *    None.                                                           *
 *  Returns:                                                          *
 *     0 = Normal.                                                    *
 *    -1 = Error, accept error.                                       *
 *    -2 = Error, connection descriptors are already full.            *
 **********************************************************************/
static int AcceptPalert( void )
{
	int  cliport;
	int  connsd;
	char connip[INET6_ADDRSTRLEN];

	uint32_t i;

	struct epoll_event      acceptevt;
	struct sockaddr_storage cliaddr;
	struct sockaddr_in     *cli4_ptr = (struct sockaddr_in *)&cliaddr;
	struct sockaddr_in6    *cli6_ptr = (struct sockaddr_in6 *)&cliaddr;
	uint32_t                clilen = sizeof(cliaddr);
	CONNDESCRIP            *tmpconn = NULL;


	acceptevt.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
	acceptevt.data.ptr = NULL;


	if ( (connsd = accept(AcceptSocket, (struct sockaddr *)&cliaddr, &clilen)) == -1 ) {
		printf("palert2ew: Accepted new Palert's connection error!\n");
		return -1;
	}

	switch (cliaddr.ss_family) {
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
	for ( i=0; i<_MaxStationNum; i++ ) {
		tmpconn = PalertConn + i;

		if ( tmpconn->sd == -1 ) {
			int tmp = i % 2;

			tmpconn->sd = connsd;
			tmpconn->syncErrCnt = 0;
			strcpy(tmpconn->ip, connip);
			time(&tmpconn->lastAct);

			acceptevt.data.ptr = tmpconn;
			epoll_ctl(EpollFd[tmp], EPOLL_CTL_ADD, tmpconn->sd, &acceptevt);

			printf("palert2ew: New Palert connection from %s:%d.\n", connip, cliport);
			break;
		}
	}

	if ( i == _MaxStationNum ) {
		printf("palert2ew: Palert connection is full. Drop connection from %s:%d.\n", connip, cliport);
		close(connsd);
		return -2;
	}

	return 0;
}


/*************************************************************
 *  CheckPalertConn( ) -- Check connections of all Palerts.  *
 *  Arguments:                                               *
 *    None.                                                  *
 *  Returns:                                                 *
 *    >0 = Refer to the total Palert connections number.     *
 *************************************************************/
int CheckPalertConn( void )
{
	uint32_t i;
	uint32_t count = 0;
	time_t   timenow;

	CONNDESCRIP *tmpconn = NULL;

	time(&timenow);

	for ( i=0; i<_MaxStationNum; i++ ) {
		tmpconn = PalertConn + i;
		if ( tmpconn->sd != -1 ) {
			if ( (timenow - tmpconn->lastAct) >= 120 ) {
				logit("t", "palert2ew: Connection: %s idle over two minutes, close connection!\n", tmpconn->ip);
				ClosePalertConn( tmpconn, i % 2 );
			}

			if ( tmpconn->isPalert ) count++;
		}
	}

	return count;
}


/********************************************************************
 *  ClosePalertConn( ) -- Close the connect of the Palert.          *
 *  Arguments:                                                      *
 *    conn  = Pointer to the connection descriptor needed to close. *
 *    index = Index for EpollFd.                                    *
 *  Returns:                                                        *
 *    None.                                                         *
 ********************************************************************/
static void ClosePalertConn( CONNDESCRIP *conn, unsigned int index )
{
	if ( conn->sd != -1 ) {
		struct epoll_event tmpev;

		tmpev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLET;
		tmpev.data.ptr = conn;

		epoll_ctl(EpollFd[index], EPOLL_CTL_DEL, conn->sd, &tmpev);

		close(conn->sd);

		memset(conn, 0, sizeof(CONNDESCRIP));
		conn->sd = -1;
	}
	return;
}


/********************************************************************
 *  GetAcceptSocket( ) -- Return the accept socket number.          *
 *  Arguments:                                                      *
 *    None                                                          *
 *  Returns:                                                        *
 *    >0 = Normal, refer to the socket number.                      *
 *    -1 = Error, reconstruct socket error.                         *
 ********************************************************************/
int GetAcceptSocket( void )
{
	return AcceptSocket;
}


/*********************************************************
 *  EndPalertServer( ) -- End process of Palert server.  *
 *  Arguments:                                           *
 *    None.                                              *
 *  Returns:                                             *
 *    None.                                              *
 *********************************************************/
void PalertServerEnd( void )
{
	uint32_t i;

	logit("o", "palert2ew: Closing all the connections of Palerts!\n");
	close(AcceptSocket);

/* Closing connections of Palerts */
	for ( i=0; i<_MaxStationNum; i++ ) ClosePalertConn( (PalertConn + i), i % 2 );
	free(PalertConn);

/* Free epoll & readevts */
	for ( i=0; i<2; i++ ) {
		close(EpollFd[i]);
		free(ReadEvts[i]);
		free(Buffer[i]);
	}

	return;
}
