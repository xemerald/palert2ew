/*
 *
 */
/* Standard C header include */
#include <stdint.h>
#include <string.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <mem_circ_queue.h>
/* Local header include */
#include <palert2ew.h>


/* Internal stack related struct */
struct sta_buffer {
	void    *sptr;
	uint8_t  data[PALERTMODE1_PACKET_LENGTH];
};
/* */
struct sta_buffer_param {
	uint8_t  header_ready;
	uint16_t buffer_rear;
};
/* */
typedef struct {
	struct sta_buffer       buf;
	struct sta_buffer_param param;
} STA_BUFFER;

/* Define global variables */
static pthread_mutex_t QueueMutex;
static QUEUE           MsgQueue;         /* from queue.h, queue.c; sets up linked */
static MSG_LOGO        RawLogo = { 0 };  /* Raw packet logo for module, type, instid */

/* */
static int validate_serial_pah1( const PALERTMODE1_HEADER *, const int );

/*
 * pa2ew_msgqueue_init() - Initialization function of message queue and mutex.
 */
int pa2ew_msgqueue_init( const unsigned long queue_size, const unsigned long element_size, const MSG_LOGO raw_logo )
{
/* Create a Mutex to control access to queue */
	CreateSpecificMutex(&QueueMutex);
/* Initialize the message queue */
	initqueue( &MsgQueue, queue_size, element_size + 1 );
/* */
	RawLogo = raw_logo;

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
int pa2ew_msgqueue_dequeue( void *buffer, size_t *size, MSG_LOGO *logo )
{
	int      result;
	long int _size;

	RequestSpecificMutex(&QueueMutex);
	result = dequeue(&MsgQueue, (char *)buffer, &_size, logo);
	ReleaseSpecificMutex(&QueueMutex);
	*size = _size;

	return result;
}

/*
 * pa2ew_msgqueue_enqueue() - Put the compelete packet into the main queue.
 */
int pa2ew_msgqueue_enqueue( void *buffer, size_t size, MSG_LOGO logo )
{
	int result = 0;

/* put it into the main queue */
	RequestSpecificMutex(&QueueMutex);
	result = enqueue(&MsgQueue, (char *)buffer, size, logo);
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
 * pa2ew_msgqueue_rawpacket() - Stack received message into queue of station.
 */
/* Internal macro for pa2ew_msgqueue_rawpacket() */
#define RESET_BUFFER_IN_STA(STA_BUF) \
		((STA_BUF)->param.buffer_rear = 0)

#define GET_FRAG_BYTES_OF_200BLOCK_IN_STA(STA_BUF) \
		((STA_BUF)->param.buffer_rear % PALERTMODE1_HEADER_LENGTH)

#define ENBUFFER_IN_STA(STA_BUF, INPUT, SIZE) \
		__extension__({ \
			memcpy((STA_BUF)->buf.data + (STA_BUF)->param.buffer_rear, (INPUT), (SIZE)); \
			(STA_BUF)->param.buffer_rear += (SIZE); \
		})

#define VALIDATE_LATEST_200BLOCK_IN_STA(STAINFO) \
		__extension__({ \
			int                      _ret_in_MACRO   = 0; \
			STA_BUFFER              *_sbuf_in_MACRO  = (STA_BUF *)(STAINFO)->msg_buffer;
			struct sta_buffer_param *_param_in_MACRO = &_sbuf_in_MACRO->param; \
			PALERTMODE1_HEADER      *_pah_in_MACRO   = \
				(PALERTMODE1_HEADER *)(_sbuf_in_MACRO->buf.data + _param_in_MACRO->buffer_rear); \
			if ( (_ret_in_MACRO = validate_serial_pah1( --_pah_in_MACRO, (STAINFO)->serial )) > 0 ) { \
				if ( _ret_in_MACRO == PALERTMODE1_PACKET_LENGTH ) { \
					if ( _param_in_MACRO->buffer_rear > PALERTMODE1_HEADER_LENGTH ) { \
						memmove(_sbuf_in_MACRO->buf.data, _pah_in_MACRO, PALERTMODE1_HEADER_LENGTH); \
						_param_in_MACRO->buffer_rear = PALERTMODE1_HEADER_LENGTH; \
					} \
					_param_in_MACRO->header_ready = 1; \
				} \
				else { \
					_param_in_MACRO->buffer_rear -= PALERTMODE1_HEADER_LENGTH; \
				} \
			} \
			else if ( !_param_in_MACRO->header_ready ) { \
				goto tcp_error; \
			} \
		})

/* Real function for pa2ew_msgqueue_rawpacket() */
int pa2ew_msgqueue_rawpacket( _STAINFO *stainfo, const void *raw_packet, const size_t packet_len )
{
	const uint8_t            *src  = raw_packet;
	const PALERTMODE1_HEADER *pah  = (PALERTMODE1_HEADER *)src;
	STA_BUFFER               *sbuf = (STA_BUFFER *)stainfo->msg_buffer;

	size_t data_remain = packet_len;
	size_t data_in     = 0;
	int    result      = 0;

/* Create the buffer for ezch station */
	if ( sbuf == NULL ) {
		stainfo->msg_buffer = (STA_BUFFER *)calloc(1, sizeof(STA_BUFFER));
		sbuf = (STA_BUFFER *)stainfo->msg_buffer;
	}
/* */
	while ( data_remain >= PALERTMODE1_HEADER_LENGTH ) {
	/* */
		data_in = PALERTMODE1_HEADER_LENGTH;
		int tmp = validate_serial_pah1( pah, stainfo->serial );
	/* */
		if ( tmp > 0 ) {
			if ( tmp == PALERTMODE1_PACKET_LENGTH ) {
				RESET_BUFFER_IN_STA( sbuf );
				ENBUFFER_IN_STA( sbuf, src, data_in );
			/* */
				sbuf->param.header_ready = 1;
			}
		}
		else {
			if ( sbuf->param.buffer_rear ) {
			/* */
				data_in -= GET_FRAG_BYTES_OF_200BLOCK_IN_STA( stainfo );
				ENBUFFER_IN_STA( sbuf, src, data_in );
			/* */
				if ( data_in != PALERTMODE1_HEADER_LENGTH && !GET_FRAG_BYTES_OF_200BLOCK_IN_STA( sbuf ) )
					VALIDATE_LATEST_200BLOCK_IN_STA( stainfo );
			/* */
				if ( sbuf->param.header_ready ) {
					if ( sbuf->param.buffer_rear == PALERTMODE1_PACKET_LENGTH ) {
					/* Put it into the main queue */
						result = pa2ew_msgqueue_enqueue( sbuf, sizeof(struct sta_buffer), RawLogo );
					/* Flush the queue of station */
						RESET_BUFFER_IN_STA( sbuf );
						sbuf->param.header_ready = 0;
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
		ENBUFFER_IN_STA( sbuf, src, data_remain );
		src        += data_remain;
		data_remain = 0;
		if ( !GET_FRAG_BYTES_OF_200BLOCK_IN_STA( sbuf ) )
			VALIDATE_LATEST_200BLOCK_IN_STA( stainfo );
	}

	return result;

tcp_error:
	RESET_BUFFER_IN_STA( sbuf );
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
