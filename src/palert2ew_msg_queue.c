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

static int filter_ntp_sync_pah1( const PALERTMODE1_HEADER *pah, _STAINFO *stainfo );

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
 * pa2ew_msgqueue_end() - End process of message queue.
 */
void pa2ew_msgqueue_end( void )
{
	freequeue(&MsgQueue);
	CloseSpecificMutex(&QueueMutex);

	return;
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
 * pa2ew_msgqueue_enqueue() - Put the compelete packet into the main queue.
 */
int pa2ew_msgqueue_enqueue( PACKET *packet, size_t size )
{
	int ret = 0;

/* put it into the main queue */
	RequestSpecificMutex(&QueueMutex);
	ret = enqueue(&MsgQueue, (char *)packet, size, (MSG_LOGO){ 0 });
	ReleaseSpecificMutex(&QueueMutex);

	if ( ret != 0 ) {
		if ( ret == -1 )
			logit("e", "palert2ew: Main queue cannot allocate memory, lost message!\n");
		else if ( ret == -2 )
			logit("e", "palert2ew: Unknown error happened to main queue!\n");
		else if ( ret == -3 )
			logit("e", "palert2ew: Main queue has lapped, please check it!\n");
	}

	return ret;
}

/*
 * pa2ew_msgqueue_prequeue() - Stack received message into queue of station.
 */
int pa2ew_msgqueue_prequeue( _STAINFO *stainfo, const PREPACKET *pre_packet )
{
	int ret = 0;

	size_t data_en   = 0;


	PACKET             *out_packet = &stainfo->packet;
	PACKETPARAM        *param      = &stainfo->param;
	PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)out_packet->data;
	//PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)pre_packet->data;

	size_t         data_remain = pre_packet->len;
	const uint8_t *src         = pre_packet->data;
	uint8_t       *dest        = out_packet->data;
	pah = (PALERTMODE1_HEADER *)pre_packet->data;

	while ( data_remain >= PALERTHEADER_LENGTH ) {
		if ( (ret = validate_serial_pah1( pah, stainfo->serial )) > 0 ) {
			if ( ret == PALERTPACKET_LENGTH ) {
				param->packet_rear = 0;
				memcpy(&out_packet->data[param->packet_rear], &pre_packet->data[data_en], PALERTHEADER_LENGTH);
				data_en            += PALERTHEADER_LENGTH;
				param->packet_rear += PALERTHEADER_LENGTH;
				param->header_ready = 1;
			}
			else {
			/* Palceholder */
			}
		}
		else {
			if ( (ret = param->packet_rear % PALERTHEADER_LENGTH) ) {
				ret = PALERTHEADER_LENGTH - ret;
				memcpy(&out_packet->data[param->packet_rear], &pre_packet->data[data_en], ret);
				data_en            += ret;
				(uint8_t *)pah     += ret;
				data_remain        -= ret;
				param->packet_rear += ret;
				PALERTMODE1_HEADER *tmppah = (PALERTMODE1_HEADER *)&out_packet->data[param->packet_rear - PALERTHEADER_LENGTH];
				if ( (ret = validate_serial_pah1( tmppah, stainfo->serial )) > 0 ) {
					if ( ret == PALERTPACKET_LENGTH ) {
						if ( param->packet_rear > PALERTHEADER_LENGTH ) {
							memmove();
							param->packet_rear = PALERTHEADER_LENGTH;
						}
						param->header_ready = 1;
					}
					else {
						param->packet_rear -= PALERTHEADER_LENGTH;
					}
				}
				else if ( !param->header_ready ) {
					param->packet_rear = 0;
				}
				continue;
			}
			else if ( !param->header_ready && param->packet_rear ) {
				PALERTMODE1_HEADER *tmppah = (PALERTMODE1_HEADER *)&out_packet->data;
				if ( (ret = validate_serial_pah1( tmppah, stainfo->serial )) > 0 ) {
					if ( ret == PALERTPACKET_LENGTH )
						param->header_ready = 1;
					else
						param->packet_rear = 0;
				}
				else {
					param->packet_rear = 0;
				}
			}

			if ( param->header_ready ) {
				memcpy(&out_packet->data[param->packet_rear], &pre_packet->data[data_en], PALERTHEADER_LENGTH);
				data_en            += PALERTHEADER_LENGTH;
				param->packet_rear += PALERTHEADER_LENGTH;
				if ( param->packet_rear == PALERTPACKET_LENGTH ) {
				/* Flush the queue of station */
					param->packet_rear  = 0;
					param->header_ready = 0;
				/* Put the raw data to the raw ring */
					if ( RawRegion->key > 0 )
						if ( tport_putmsg( RawRegion, RawLogo, PALERTPACKET_LENGTH, (char *)out_packet->data ) != PUT_OK )
							logit( "e", "palert2ew: Error putting message in region %ld\n", RawRegion->key );

				/* put it into the main queue */
					if ( filter_ntp_sync_pah1( pah, stainfo ) )
						ret = pa2ew_msgqueue_enqueue( out_packet, sizeof(PACKET) );
					else
						continue;
				}
			}
		}
		pah++;
		data_remain -= PALERTHEADER_LENGTH;
	}

	if ( data_remain ) {
		memcpy(&out_packet->data[param->packet_rear], &pre_packet->data[data_en], data_remain);
		data_en            += data_remain;
		param->packet_rear += data_remain;
	}

/* Process retrieved msg, and the following is origin from dayi's code */
	do {
	/* If there is no data inside queue of station, require total header length, 200 bytes */
		if ( param->packet_rear == 0 )
			param->packet_req = PALERTHEADER_LENGTH;
	/* Reach the required data length */
		if ( data_read >= param->packet_req ) {
			memcpy(&out_packet->data[param->packet_rear], &pre_packet->data[data_en], param->packet_req);
			data_read          -= param->packet_req;
			data_en            += param->packet_req;
			param->packet_rear += param->packet_req;

		/* Reach the wave data rear, send to the main queue */
			if ( param->packet_rear == PALERTPACKET_LENGTH ) {
			/* Flush the queue of station */
				param->packet_rear = 0;

			/* Put the raw data to the raw ring */
				if ( RawRegion->key > 0 )
					if ( tport_putmsg( RawRegion, RawLogo, PALERTPACKET_LENGTH, (char *)out_packet->data ) != PUT_OK )
						logit( "e", "palert2ew: Error putting message in region %ld\n", RawRegion->key );

			/* put it into the main queue */
				if ( filter_ntp_sync_pah1( pah, stainfo ) )
					ret = pa2ew_msgqueue_enqueue( out_packet, sizeof(PACKET) );
				else
					continue;
			}
			else {
				if ( (ret = validate_serial_pah1( pah, stainfo->serial )) > 0 ) {
					if ( ret > PALERTHEADER_LENGTH )
						param->packet_req = PALERTPACKET_LENGTH - PALERTHEADER_LENGTH;
					else
						param->packet_rear = 0;
				}
				else {
					goto tcp_error;
				}
			}
		}
		else {
			memcpy(&out_packet->data[param->packet_rear], &pre_packet->data[data_en], data_read);
			param->packet_req  -= data_read;
			param->packet_rear += data_read;
			data_read = 0;
		}
	} while ( data_read > 0 );

	return ret;
tcp_error:
	param->packet_rear = 0;
	return 1;
}

/*
 *
 */
static int validate_serial_pah1( const PALERTMODE1_HEADER *pah, const int serial )
{
	if ( PALERTMODE1_HEADER_CHECK_SYNC( pah ) ) {
		if ( PALERTMODE1_HEADER_GET_SERIAL(pah) == (uint16_t)serial ) {
			if ( PALERTMODE1_HEADER_GET_PACKETLEN( pah ) == PALERTPACKET_LENGTH )
				return PALERTPACKET_LENGTH;
			else if ( PALERTMODE1_HEADER_GET_PACKETLEN( pah ) == PALERTHEADER_LENGTH )
				return PALERTHEADER_LENGTH;
		}
	}

	return -1;
}

/*
 *
 */
static int filter_ntp_sync_pah1( const PALERTMODE1_HEADER *pah, _STAINFO *stainfo )
{
	uint8_t *ntp_errors = &stainfo->param->ntp_errors;

/* Check NTP SYNC. */
	if ( PALERTMODE1_HEADER_CHECK_NTP( pah ) ) {
		if ( *ntp_errors >= PA2EW_NTP_SYNC_ERR_LIMIT )
			logit("o", "palert2ew: Station %s NTP resync, now back online.\n", stainfo->sta);
		*ntp_errors = 0;
	}
	else {
		if ( *ntp_errors >= 25 ) {
			if ( *ntp_errors < PA2EW_NTP_SYNC_ERR_LIMIT ) {
				printf("palert2ew: Station %s NTP sync error, please check it!\n", stainfo->sta);
			}
			else {
				if ( *ntp_errors == PA2EW_NTP_SYNC_ERR_LIMIT )
					logit("e", "palert2ew: Station %s NTP sync error, drop the packet.\n", stainfo->sta);
				return 0;
			}
		}
		(*ntp_errors)++;
	}

	return 1;
}

/*
 *
 */
static size_t enbuffer_station( _STAINFO *stainfo, const void *input, const size_t size )
{
	memcpy(&stainfo->packet->data[stainfo->param->packet_rear], input, size);
	stainfo->param->packet_rear += size;

	return size;
}
