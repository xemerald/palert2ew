/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Earthworm environment header include */
#include <earthworm.h>
#include <transport.h>
#include <mem_circ_queue.h>

/* Local header include */
#include <palert.h>
#include <palert2ew.h>

/* Define global variables */
static pthread_mutex_t QueueMutex;
static QUEUE           MsgQueue;    /* from queue.h, queue.c; sets up linked */
static const SHM_INFO *RawRegion = NULL;  /* shared memory region to use for raw i/o    */
static const MSG_LOGO *RawLogo   = NULL;

/*
 * pa2ew_msgqueue_init() - Initialization function of message queue and mutex.
 */
int pa2ew_msgqueue_init( const unsigned long queue_size, const SHM_INFO *region, const MSG_LOGO *logo )
{
/* Create a Mutex to control access to queue */
	CreateSpecificMutex(&QueueMutex);
/* Initialize the message queue */
	initqueue( &MsgQueue, queue_size, (unsigned long)sizeof(PACKET) + 1 );
/* Fill in the raw region & logo */
	RawRegion = region;
	RawLogo   = logo;

	return 0;
}

/*
 * pa2ew_msgqueue_dequeue() - Pop-out received message from main queue.
 */
int pa2ew_msgqueue_dequeue( PACKET *packet, size_t *size )
{
	int      ret;
	MSG_LOGO dummy;

	RequestSpecificMutex(&QueueMutex);
	ret = dequeue(&MsgQueue, (char *)packet, size, &dummy);
	ReleaseSpecificMutex(&QueueMutex);

	return ret;
}

/*
 * pa2ew_msgqueue_enqueue() - Stack received message into queue of station
 *                            or main queue.
 */
int pa2ew_msgqueue_enqueue( PREPACKET *packetIn, _STAINFO *staPtr )
{
	int ret = 0;

	uint32_t dataRead = packetIn->len;
	uint32_t dataEnPacket = 0;

	PACKET             *packetOut   = &staPtr->packet;
	PACKETPARAM        *packetParam = &staPtr->param;
	PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)packetOut->data;

/* Process retrieved msg */
	do
/* The following is dayi's code */
	{
	/* If there is no data inside queue of station,
	   require total header length, 200 bytes. */
		if ( packetParam->PacketRear == 0 ) {
			packetParam->PacketReq = PALERTHEADER_LENGTH;
		}

	/* Reach the required data length */
		if ( dataRead >= packetParam->PacketReq ) {
			memcpy(&packetOut->data[packetParam->PacketRear], &packetIn->data[dataEnPacket], packetParam->PacketReq);

			dataRead -= packetParam->PacketReq;
			dataEnPacket += packetParam->PacketReq;
			packetParam->PacketRear += packetParam->PacketReq;

		/* Reach the wave data rear, send to the main queue */
			if ( packetParam->PacketRear == PALERTPACKET_LENGTH ) {
			/* Flush the queue of station */
				packetParam->PacketRear = 0;

			/* Put the raw data to the raw ring */
				if ( RawRegion->key > 0 ) {
					if ( tport_putmsg( RawRegion, RawLogo, PALERTPACKET_LENGTH, (char *)packetOut->data ) != PUT_OK ) {
						logit( "e", "palert2ew: Error putting message in region %ld\n", RawRegion->key );
					}
				}

			/* Check NTP SYNC. */
				if ( PALERTMODE1_HEADER_CHECK_NTP( pah ) ) {
					if ( packetParam->NtpErrCnt >= 30 ) {
						logit("o", "palert2ew: Station %s NTP resync, now back online.\n", staPtr->sta_c);
					}
					packetParam->NtpErrCnt = 0;
				}
				else {
					if ( packetParam->NtpErrCnt >= 25 ) {
						if ( packetParam->NtpErrCnt <= 30 ) {
							if ( packetParam->NtpErrCnt == 30 ) {
								logit("e", "palert2ew: Station %s NTP sync error, drop the packet.\n", staPtr->sta_c);
							}
							else printf("palert2ew: Station %s NTP sync error, please check it!\n", staPtr->sta_c);
						}
						else continue;
					}
					packetParam->NtpErrCnt++;
				}

			/* put it into the main queue */
				RequestSpecificMutex(&QueueMutex);
				ret = enqueue(&MsgQueue, (char *)packetOut, sizeof(PACKET), (MSG_LOGO){ 0 });
				ReleaseSpecificMutex(&QueueMutex);

				if ( ret != 0 ) {
					if ( ret == -2 ) {
					/* Currently, eneueue() in mem_circ_queue.c never returns this error. */
						ret = -3;
					}
					else if ( ret == -1 ) {
					/* Queue cannot allocate memory, lost message. */
						ret = -2;
					}
					else if ( ret == -3 ) {
					/* Queue is lapped too often to be logged to screen.
					 * Log circular queue laps to logfile.
					 * Maybe queue laps should not be logged at all.
					 */
						ret = -4;
					}
				}
			}
			else {
			/* Check connection sync. */
				if ( PALERTMODE1_HEADER_CHECK_SYNC( pah ) ) {
				/* 1200 bytes message */
					if ( PALERTMODE1_HEADER_GET_PACKETLEN( pah ) == PALERTPACKET_LENGTH ) {
						packetParam->PacketReq = PALERTPACKET_LENGTH - PALERTHEADER_LENGTH;
					}
				/* 200 bytes message */
					else if ( PALERTMODE1_HEADER_GET_PACKETLEN( pah ) == PALERTHEADER_LENGTH ) {
						packetParam->PacketRear = 0;
					}
				/* TCP connection sync error */
					else {
						packetParam->PacketRear = 0;
						return -1;
					}
				}
			/* TCP connection sync error */
				else {
					packetParam->PacketRear = 0;
					return -1;
				}
			}
		}
		else {
			memcpy(&packetOut->data[packetParam->PacketRear], &packetIn->data[dataEnPacket], dataRead);
			packetParam->PacketReq -= dataRead;
			packetParam->PacketRear += dataRead;
			dataRead = 0;
		}
	} while ( dataRead > 0 );

	return ret;
}

/************************************************
 *  MsgQueueEnd( ) -- End process of message    *
 *                    queue.                    *
 *  Arguments:                                  *
 *    None.                                     *
 *  Returns:                                    *
 *    None.                                     *
 ************************************************/
void MsgQueueEnd( void )
{
	freequeue(&MsgQueue);
	CloseSpecificMutex(&QueueMutex);

	return;
}
