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
#include <palert2ew_list.h>

/* Internal stack related struct */
struct last_buffer {
	uint16_t buffer_rear;
	uint8_t  buffer[PA2EW_RECV_BUFFER_LENGTH];
};

/* Define global variables */
static mutex_t  QueueMutex;
static QUEUE    MsgQueue;         /* from queue.h, queue.c; sets up linked */
static size_t   LRBufferOffset = 0;

/* */
static void                 save_last_buffer( void *, const size_t );
static LABELED_RECV_BUFFER *draw_last_buffer( void *, size_t * );
static struct last_buffer  *create_last_buffer( _STAINFO * );
static void                 free_last_buffer_act( void *, const int, void * );
static int pre_enqueue_check_pah1( LABELED_RECV_BUFFER *, size_t *, MSG_LOGO );
static int pre_enqueue_check_pah4( LABELED_RECV_BUFFER *, size_t *, MSG_LOGO );
static int validate_serial_pah1( const PALERTMODE1_HEADER *, const int );
static int validate_serial_pah4( const PALERTMODE4_HEADER *, const int );

/*
 * pa2ew_msgqueue_init() - Initialization function of message queue and mutex.
 */
int pa2ew_msgqueue_init( const unsigned long queue_size, const unsigned long element_size )
{
	LABELED_RECV_BUFFER _lrbuf;

/* Create a Mutex to control access to queue */
	CreateSpecificMutex(&QueueMutex);
/* Initialize the message queue */
	initqueue( &MsgQueue, queue_size, element_size + 1 );
/* Initialize the labeled buffer real offset */
	LRBufferOffset = _lrbuf.recv_buffer - (uint8_t *)&_lrbuf;

	return 0;
}

/*
 * pa2ew_msgqueue_end() - End process of message queue.
 */
void pa2ew_msgqueue_end( void )
{
	RequestSpecificMutex(&QueueMutex);
	freequeue(&MsgQueue);
	ReleaseSpecificMutex(&QueueMutex);
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
			logit("et", "palert2ew: Main queue cannot allocate memory, lost message!\n");
		else if ( result == -2 )
			logit("et", "palert2ew: Unknown error happened to main queue!\n");
		else if ( result == -3 )
			logit("et", "palert2ew: Main queue has lapped, please check it!\n");
	}

	return result;
}

/*
 * pa2ew_msgqueue_rawpacket() - Stack received message into queue of station.
 */
int pa2ew_msgqueue_rawpacket( void *label_buf, size_t buf_len, int packet_type, MSG_LOGO logo )
{
	LABELED_RECV_BUFFER *lrbuf;
	int                  sync_flag = 0;

/* */
	lrbuf = draw_last_buffer( label_buf, &buf_len );
/* */
	if ( packet_type == 1 )
		sync_flag = pre_enqueue_check_pah1( lrbuf, &buf_len, logo );
	else if ( packet_type == 4 )
		sync_flag = pre_enqueue_check_pah4( lrbuf, &buf_len, logo );
	else
		buf_len = 0;
/* Try to store the remainder data into the buffer */
	if ( buf_len )
		save_last_buffer( lrbuf, buf_len );
/* */
	if ( lrbuf != (LABELED_RECV_BUFFER *)label_buf )
		free(lrbuf);
/* If it did sync. return no error with 0, otherwise return error with -1. */
	return sync_flag ? 0 : -1;
}

/*
 * pa2ew_msgqueue_lastbufs_reset() - Stack received message into queue of station.
 */
void pa2ew_msgqueue_lastbufs_reset( void )
{
/* */
	pa2ew_list_walk( free_last_buffer_act, NULL );
/* */
	return;
}

/*
 *
 */
static void save_last_buffer( void *label_buf, const size_t buf_len )
{
	LABELED_RECV_BUFFER *lrbuf   = (LABELED_RECV_BUFFER *)label_buf;
	struct last_buffer *_lastbuf = (struct last_buffer *)((_STAINFO *)lrbuf->label.staptr)->buffer;
/* */
	if ( !_lastbuf )
		_lastbuf = create_last_buffer( (_STAINFO *)lrbuf->label.staptr );
/* */
	if ( buf_len < PA2EW_RECV_BUFFER_LENGTH && _lastbuf ) {
		memcpy(_lastbuf->buffer, lrbuf->recv_buffer, buf_len);
		_lastbuf->buffer_rear = buf_len;
	}

	return;
}

/*
 *
 */
static LABELED_RECV_BUFFER *draw_last_buffer( void *label_buf, size_t *buf_len )
{
	LABELED_RECV_BUFFER *result   = (LABELED_RECV_BUFFER *)label_buf;
	struct last_buffer  *_lastbuf = (struct last_buffer *)((_STAINFO *)result->label.staptr)->buffer;

/* First, deal the remainder data from last packet */
	if ( _lastbuf != NULL && _lastbuf->buffer_rear ) {
	/* */
		if ( (*buf_len + _lastbuf->buffer_rear) <= PA2EW_RECV_BUFFER_LENGTH ) {
			memmove(result->recv_buffer + _lastbuf->buffer_rear, result->recv_buffer, *buf_len);
			memcpy(result->recv_buffer, _lastbuf->buffer, _lastbuf->buffer_rear);
		}
	/* */
		else {
			LABELED_RECV_BUFFER *_result =
				(LABELED_RECV_BUFFER *)malloc(sizeof(LABELED_RECV_BUFFER) + _lastbuf->buffer_rear + 1);

			_result->label = result->label;
			memcpy(_result->recv_buffer, _lastbuf->buffer, _lastbuf->buffer_rear);
			memcpy(_result->recv_buffer + _lastbuf->buffer_rear, result->recv_buffer, *buf_len);
			result = _result;
		}
	/* */
		*buf_len += _lastbuf->buffer_rear;
		_lastbuf->buffer_rear = 0;
	}
	else if ( _lastbuf != NULL && !_lastbuf->buffer_rear ) {
		free(_lastbuf);
		((_STAINFO *)result->label.staptr)->buffer = NULL;
	}

	return result;
}

/*
 *
 */
static struct last_buffer *create_last_buffer( _STAINFO *staptr )
{
	struct last_buffer *result = (struct last_buffer *)calloc(1, sizeof(struct last_buffer));

	result->buffer_rear = 0;
	staptr->buffer = result;

	return result;
}

/*
 *
 */
static void free_last_buffer_act( void *node, const int index, void *arg )
{
	_STAINFO           *staptr  = (_STAINFO *)node;
	struct last_buffer *lastbuf = (struct last_buffer *)staptr->buffer;
/* */
	if ( lastbuf ) {
		lastbuf->buffer_rear = 0;
		free(lastbuf);
		staptr->buffer = NULL;
	}

	return;
}

/*
 *
 */
static int pre_enqueue_check_pah1( LABELED_RECV_BUFFER *lrbuf, size_t *buf_len, MSG_LOGO logo )
{
	uint16_t            serial = ((_STAINFO *)lrbuf->label.staptr)->serial;
	PALERTMODE1_HEADER *pah;
/* */
	int    ret           = 0;
	int    sync_flag     = 0;
	size_t header_offset = 0;

/* */
	for (
		pah = (PALERTMODE1_HEADER *)lrbuf->recv_buffer;
		*buf_len >= PALERTMODE1_HEADER_LENGTH;
		*buf_len -= PALERTMODE1_HEADER_LENGTH, pah++
	) {
	/* Testing for the mode 1 packet header */
		if ( (ret = validate_serial_pah1( pah, serial )) > 0 ) {
			if ( ret == PALERTMODE1_PACKET_LENGTH ) {
			/*
			 * Once the mode 1 packet header is incoming,
			 * we should flush the existed buffer and move the
			 * incoming header to beginning of buffer.
			 */
				if ( (uint8_t *)pah != lrbuf->recv_buffer ) {
					memmove(lrbuf->recv_buffer, pah, *buf_len);
					pah = (PALERTMODE1_HEADER *)lrbuf->recv_buffer;
				}
				header_offset = PALERTMODE1_HEADER_LENGTH;
			}
		/* */
			else if ( header_offset ) {
				memmove(pah, pah + 1, *buf_len - PALERTMODE1_HEADER_LENGTH);
				pah--;
			}
			sync_flag = 1;
		}
	/* */
		else if ( header_offset ) {
			header_offset += PALERTMODE1_HEADER_LENGTH;
		/* Reach the required mode 1 packet length */
			if ( header_offset == PALERTMODE1_PACKET_LENGTH ) {
				if ( pa2ew_msgqueue_enqueue( lrbuf, PALERTMODE1_PACKET_LENGTH + LRBufferOffset, logo ) )
					sleep_ew(50);
				header_offset = 0;
			}
		}
	}
/* */
	if ( header_offset )
		*buf_len += header_offset;
	else if ( *buf_len )
		memmove(lrbuf->recv_buffer, pah, *buf_len);

	return sync_flag;
}

/*
 *
 */
static int pre_enqueue_check_pah4( LABELED_RECV_BUFFER *lrbuf, size_t *buf_len, MSG_LOGO logo )
{
	uint16_t            serial = ((_STAINFO *)lrbuf->label.staptr)->serial;
	PALERTMODE4_HEADER *pah4   = (PALERTMODE4_HEADER *)lrbuf->recv_buffer;
/* */
	int ret       = 0;
	int sync_flag = 0;

/* */
	do {
		if ( (ret = validate_serial_pah4( pah4, serial )) > 0 ) {
		/* */
			sync_flag = 1;
		/* */
			if ( (uint8_t *)pah4 != lrbuf->recv_buffer ) {
				memmove(lrbuf->recv_buffer, pah4, *buf_len);
				pah4 = (PALERTMODE4_HEADER *)lrbuf->recv_buffer;
			}
		/* */
			if ( *buf_len >= (size_t)ret ) {
				if ( pa2ew_msgqueue_enqueue( lrbuf, ret + LRBufferOffset, logo ) )
					sleep_ew(50);
			/* */
				*buf_len -= ret;
				pah4 = (PALERTMODE4_HEADER *)(lrbuf->recv_buffer + ret);
			}
			else {
				break;
			}
		}
		else {
			pah4++;
			*buf_len -= sizeof(PALERTMODE4_HEADER);
		}
	} while ( *buf_len > sizeof(PALERTMODE4_HEADER) );

	if ( *buf_len && (uint8_t *)pah4 != lrbuf->recv_buffer )
		memmove(lrbuf->recv_buffer, pah4, *buf_len);

	return sync_flag;
}

/*
 * validate_serial_pah1() -
 */
static int validate_serial_pah1( const PALERTMODE1_HEADER *pah, const int serial )
{
	if ( PALERTMODE1_HEADER_CHECK_SYNC( pah ) ) {
		if ( PALERTMODE1_HEADER_GET_SERIAL( pah ) == (uint16_t)serial ) {
			if ( PALERTMODE1_HEADER_GET_PACKETLEN( pah ) == PALERTMODE1_PACKET_LENGTH )
				return PALERTMODE1_PACKET_LENGTH;
			else if ( PALERTMODE1_HEADER_GET_PACKETLEN( pah ) == PALERTMODE1_HEADER_LENGTH )
				return PALERTMODE1_HEADER_LENGTH;
		}
	}

	return -1;
}

/*
 * validate_serial_pah4() -
 */
static int validate_serial_pah4( const PALERTMODE4_HEADER *pah4, const int serial )
{
	if ( PALERTMODE4_HEADER_CHECK_SYNC( pah4 ) ) {
		if ( PALERTMODE4_HEADER_GET_SERIAL( pah4 ) == (uint16_t)serial ) {
		/* Still need to check the CRC16 */
			return PALERTMODE4_HEADER_GET_PACKETLEN( pah4 );
		}
	}

	return -1;
}
