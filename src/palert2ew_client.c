/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
/* Earthworm environment header include */
#include <earthworm.h>
/* Local header include */
#include <palert2ew_list.h>
#include <palert2ew_socket.h>
#include <palert2ew_msg_queue.h>

#define FORWARD_PACKET_HEADER_LENGTH  4

static volatile int  ClientSocket = -1;
static const char   *_ServerIP    = NULL;
static const char   *_ServerPort  = NULL;
static uint8_t      *Buffer       = NULL;
static uint8_t       SyncErrCnt   = 0;


/********************************************************************
 *  PalertClientInit( ) -- Initialize the dependent Palert client.  *
 *  Arguments:                                                      *
 *    serverIP   = String of IP address of Palert server.           *
 *    serverPort = String of the port to connect.                   *
 *  Returns:                                                        *
 *     0 = Normal, success.                                         *
 ********************************************************************/
int PalertClientInit( const char *serverIP, const char *serverPort )
{
/* Setup constants */
	SyncErrCnt  = 0;
	_ServerIP   = serverIP;
	_ServerPort = serverPort;

/* Construct the accept socket */
	if ( (ClientSocket = ConstructSocket(_ServerIP, _ServerPort)) == -1 ) {
		return -1;
	}

/* Allocate read-in buffer */
	Buffer = calloc(1, sizeof(PREPACKET));

	return 0;
}

/******************************************************************************
 * ReadServerData() Receive the messages from the socket of forward server  *
 *                    and send it to the MessageStacker.                      *
 ******************************************************************************/
int ReadServerData( void )
{
	int ret;

	int32_t byteRec = 0;
	int32_t dataReq = FORWARD_PACKET_HEADER_LENGTH;
	int32_t dataRead = 0;
	uint16_t serial;

	PREPACKET *readptr = (PREPACKET *)Buffer;
	_STAINFO  *staptr = NULL;

	memset(Buffer, 0, PREPACKET_LENGTH);

	do
	{
		if ( (byteRec = recv(ClientSocket, Buffer + dataRead, dataReq, 0)) <= 0 ) {
			if ( errno == EINTR ) {
				sleep_ew(100);
				continue;
			}
			else if ( errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT )
				logit("e", "palert2ew: Connection to Palert server is timeout, reconnect!\n");
			else if ( byteRec == 0 )
				logit("e", "palert2ew: Connection to Palert server is closed, reconnect!\n");

			if ( ReConstructSocket(_ServerIP, _ServerPort, (int *)&ClientSocket) < 0 ) {
				return -3;
			}

			dataRead = 0;
			dataReq = FORWARD_PACKET_HEADER_LENGTH;

			continue;
		}

		dataRead += byteRec;

		if ( dataRead >= FORWARD_PACKET_HEADER_LENGTH )
			dataReq = readptr->len + FORWARD_PACKET_HEADER_LENGTH - dataRead;

		/*	printf("ByteRec:%d!\n", byteRec);
			printf("DataReq:%d!\n", dataReq);
			printf("DataRead:%d!\n", dataRead);
			printf("Length:%d!\n", readptr->len);	*/	/* Debug */

	} while ( dataReq );

/* Find which one palert */
	serial = readptr->serial;

	staptr = palert2ew_list_find( serial );

	if ( staptr == NULL ) {
	/* Serial should always larger than 0, if so send the update request */
		if ( serial > 0 ) {
			printf("palert2ew: %d not found in station list, maybe it's a new palert.\n", serial);
			return -1;
		}
		else {
			/* printf("palert2ew: Recieve keep alive packet!\n"); */ /* Debug */
		}
	}
	else {
		if ( (ret = MsgEnqueue( readptr, staptr )) ) {
			if ( ret == -1 ) {
				if ( ++SyncErrCnt >= 10 ) {
					SyncErrCnt = 0;
					logit("e", "palert2ew: TCP connection sync error, reconnect!\n");
					if ( ReConstructSocket(_ServerIP, _ServerPort, (int *)&ClientSocket) < 0 ) {
						return -3;
					}
				}
			}
			else if ( ret == -4 ) {
				printf("palert2ew: Main queue has lapped, please check it!\n");
				return -2;
			}
		}
		else SyncErrCnt = 0;
	}

	return 0;
}


/*********************************************************
 *  EndPalertClient( ) -- End process of Palert client.  *
 *  Arguments:                                           *
 *    None.                                              *
 *  Returns:                                             *
 *    None.                                              *
 *********************************************************/
void PalertClientEnd( void )
{
	close(ClientSocket);
	free(Buffer);

	return;
}
