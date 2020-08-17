/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Earthworm environment header include */
#include <earthworm.h>
#include <mem_circ_queue.h>

/* Local header include */
#include <palert2ew.h>

/* Define global variables */
static pthread_mutex_t QueueMutex;
static QUEUE           MsgQueue;    /* from queue.h, queue.c; sets up linked */

/* */
static int validate_serial_pah1( const PALERTMODE1_HEADER *, const int );

/*
 * pa2ew_msgqueue_init() - Initialization function of message queue and mutex.
 */
int pa2ew_msgqueue_init( const unsigned long queue_size )
{
/* Create a Mutex to control access to queue */
	CreateSpecificMutex(&QueueMutex);
/* Initialize the message queue */
	initqueue( &MsgQueue, queue_size, (unsigned long)sizeof(PACKET) + 1 );

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
	int      result;
	long int _size;
	MSG_LOGO dummy;

	RequestSpecificMutex(&QueueMutex);
	result = dequeue(&MsgQueue, (char *)packet, &_size, &dummy);
	ReleaseSpecificMutex(&QueueMutex);
	*size = _size;

	return result;
}

/*
 * pa2ew_msgqueue_enqueue() - Put the compelete packet into the main queue.
 */
int pa2ew_msgqueue_enqueue( PACKET *packet, size_t size )
{
	int result = 0;

/* put it into the main queue */
	RequestSpecificMutex(&QueueMutex);
	result = enqueue(&MsgQueue, (char *)packet, size, (MSG_LOGO){ 0 });
	ReleaseSpecificMutex(&QueueMutex);

	if ( result != 0 ) {
		if ( result == -1 )
			logit("e", "palert2ew: Main queue cannot allocate memory, lost message!\n");
		else if ( result == -2 )
			logit("e", "palert2ew: Unknown error happened to main queue!\n");
		else if ( result == -3 )
			logit("e", "palert2ew: Main queue has lapped, please check it!\n");
	}

	return result;
}

/*
 * pa2ew_msgqueue_prequeue() - Stack received message into queue of station.
 */
/* Internal macro for pa2ew_msgqueue_prequeue() */
#define RESET_BUFFER_IN_STA(STAINFO) \
		__extension__({ \
			(STAINFO)->param.packet_rear = 0; \
		})

#define GET_REST_BYTE_OF_200BLOCK_IN_STA(STAINFO) \
		((STAINFO)->param.packet_rear % PALERTMODE1_HEADER_LENGTH)

#define ENBUFFER_IN_STA(STAINFO, INPUT, SIZE) \
		__extension__({ \
			memcpy((STAINFO)->packet.data + (STAINFO)->param.packet_rear, (INPUT), (SIZE)); \
			(STAINFO)->param.packet_rear += (SIZE); \
		})

#define VALIDATE_LATEST_200BLOCK_IN_STA(STAINFO) \
		__extension__ ({ \
			int                 _ret_in_MACRO   = 0; \
			PACKETPARAM        *_param_in_MACRO = &(STAINFO)->param; \
			PALERTMODE1_HEADER *_pah_in_MACRO   = \
				(PALERTMODE1_HEADER *)((STAINFO)->packet.data + (_param_in_MACRO->packet_rear - PALERTMODE1_HEADER_LENGTH)); \
			if ( (_ret_in_MACRO = validate_serial_pah1( _pah_in_MACRO, (STAINFO)->serial )) > 0 ) { \
				if ( _ret_in_MACRO == PALERTMODE1_PACKET_LENGTH ) { \
					if ( _param_in_MACRO->packet_rear > PALERTMODE1_HEADER_LENGTH ) { \
						memmove((STAINFO)->packet.data, _pah_in_MACRO, PALERTMODE1_HEADER_LENGTH); \
						_param_in_MACRO->packet_rear = PALERTMODE1_HEADER_LENGTH; \
					} \
					_param_in_MACRO->header_ready = 1; \
				} \
				else { \
					_param_in_MACRO->packet_rear -= PALERTMODE1_HEADER_LENGTH; \
				} \
			} \
			else if ( !_param_in_MACRO->header_ready ) { \
				goto tcp_error; \
			} \
		})

/* Real function for pa2ew_msgqueue_prequeue() */
int pa2ew_msgqueue_prequeue( _STAINFO *stainfo, const PREPACKET *pre_packet )
{
	const uint8_t            *src = pre_packet->data;
	const PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)src;

	size_t data_remain = pre_packet->len;
	size_t data_in     = 0;
	int    result      = 0;

/* */
	while ( data_remain >= PALERTMODE1_HEADER_LENGTH ) {
	/* */
		data_in = PALERTMODE1_HEADER_LENGTH;
		int tmp = validate_serial_pah1( pah, stainfo->serial );
	/* */
		if ( tmp > 0 ) {
			if ( tmp == PALERTMODE1_PACKET_LENGTH ) {
				RESET_BUFFER_IN_STA( stainfo );
				ENBUFFER_IN_STA( stainfo, src, data_in );
			/* */
				stainfo->param.header_ready = 1;
			}
		}
		else {
			if ( stainfo->param.packet_rear ) {
			/* */
				data_in = PALERTMODE1_HEADER_LENGTH - GET_REST_BYTE_OF_200BLOCK_IN_STA( stainfo );
				ENBUFFER_IN_STA( stainfo, src, data_in );
			/* */
				if ( data_in != PALERTMODE1_HEADER_LENGTH && !GET_REST_BYTE_OF_200BLOCK_IN_STA( stainfo ) )
					VALIDATE_LATEST_200BLOCK_IN_STA( stainfo );
			/* */
				if ( stainfo->param.header_ready ) {
					if ( stainfo->param.packet_rear == PALERTMODE1_PACKET_LENGTH ) {
						PACKET *out_packet = &stainfo->packet;
					/* Flush the queue of station */
						RESET_BUFFER_IN_STA( stainfo );
						stainfo->param.header_ready = 0;
					/* Put it into the main queue */
						result = pa2ew_msgqueue_enqueue( out_packet, sizeof(PACKET) );
					}
				}
			}
		}
	/* */
		src         += data_in;
		pah          = (PALERTMODE1_HEADER *)src;
		data_remain -= data_in;
	}

	if ( data_remain ) {
		ENBUFFER_IN_STA( stainfo, src, data_remain );
		src        += data_remain;
		data_remain = 0;
		if ( !GET_REST_BYTE_OF_200BLOCK_IN_STA( stainfo ) )
			VALIDATE_LATEST_200BLOCK_IN_STA( stainfo );
	}

	return result;

tcp_error:
	RESET_BUFFER_IN_STA( stainfo );
	return 1;
}

/*
 * validate_serial_pah1() -
 */
static int validate_serial_pah1( const PALERTMODE1_HEADER *pah, const int serial )
{
	if ( PALERTMODE1_HEADER_CHECK_SYNC( pah ) ) {
		if ( PALERTMODE1_HEADER_GET_SERIAL(pah) == (uint16_t)serial ) {
			if ( PALERTMODE1_HEADER_GET_PACKETLEN( pah ) == PALERTMODE1_PACKET_LENGTH )
				return PALERTMODE1_PACKET_LENGTH;
			else if ( PALERTMODE1_HEADER_GET_PACKETLEN( pah ) == PALERTMODE1_HEADER_LENGTH )
				return PALERTMODE1_HEADER_LENGTH;
		}
	}

	return -1;
}
