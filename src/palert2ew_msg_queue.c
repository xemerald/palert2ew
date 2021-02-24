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
struct last_buffer {
	uint16_t buffer_rear;
	uint8_t  buffer[PALERTMODE1_PACKET_LENGTH];
};

/* Define global variables */
static pthread_mutex_t QueueMutex;
static QUEUE           MsgQueue;         /* from queue.h, queue.c; sets up linked */
static MSG_LOGO        RawLogo = { 0 };  /* Raw packet logo for module, type, instid */

/* */
static int pre_enqueue_check_pah1( LABELED_RECV_BUFFER *, size_t * )
static int pre_enqueue_check_pah4( LABELED_RECV_BUFFER *, size_t * )
static int validate_serial_pah1( const PALERTMODE1_HEADER *, const int );
static int validate_serial_pah4( const PALERTMODE4_HEADER *, const int );

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
int pa2ew_msgqueue_rawpacket( void *label_buf, size_t buf_len, int packet_type )
{
	LABELED_RECV_BUFFER *lrbuf   = (LABELED_RECV_BUFFER *)label_buf;
	_STAINFO            *stainfo = (_STAINFO *)lrbuf->sptr;
	struct last_buffer  *lastbuf = (struct last_buffer *)stainfo->msg_buffer;
/* */
	int sync_flag = 0;

/* */
	buf_len -= lrbuf->recv_buffer - (uint8_t *)lrbuf;
/* First, deal the remainder data from last packet */
	if ( lastbuf != NULL && lastbuf->buffer_rear ) {
		if ( lastbuf->buffer_rear + buf_len < PA2EW_RECV_BUFFER_LENGTH ) {
			memmove(lrbuf->recv_buffer + lastbuf->buffer_rear, lrbuf->recv_buffer, buf_len);
			memcpy(lrbuf->recv_buffer, lastbuf->buffer, lastbuf->buffer_rear);
		}
		else {
			LABELED_RECV_BUFFER *_lrbuf = (LABELED_RECV_BUFFER *)calloc(1, sizeof(LABELED_RECV_BUFFER));

			_lrbuf->sptr = lrbuf->sptr;
			memcpy(_lrbuf->recv_buffer, lastbuf->buffer, lastbuf->buffer_rear);
			memcpy(_lrbuf->recv_buffer + lastbuf->buffer_rear, lrbuf->recv_buffer, buf_len);
			lrbuf = _lrbuf;
		}
	/* */
		buf_len += lastbuf->buffer_rear;
		lastbuf->buffer_rear = 0;
	}
	else if ( lastbuf != NULL && !lastbuf->buffer_rear ) {
		free(stainfo->msg_buffer);
		stainfo->msg_buffer = NULL;
	}
/* */
	if ( packet_type == 1 )
		sync_flag = pre_enqueue_check_pah1( lrbuf, &buf_len );
	else if ( packet_type == 4 )
		sync_flag = pre_enqueue_check_pah4( lrbuf, &buf_len );
	else
		buf_len = 0;
/* */
	if ( buf_len ) {
	/* */
		if ( lastbuf == NULL ) {
			stainfo->msg_buffer = calloc(1, sizeof(struct last_buffer));
			lastbuf = (struct last_buffer *)stainfo->msg_buffer;
		}
	/* */
		lastbuf->buffer_rear = buf_len;
		memcpy(lastbuf->buffer, lrbuf->recv_buffer, buf_len);
	}
/* */
	if ( lrbuf != (LABELED_RECV_BUFFER *)label_buf )
		free(lrbuf);

/* */
	return sync_flag ? 0 : -1;
}

/*
 *
 */
static int pre_enqueue_check_pah1( LABELED_RECV_BUFFER *lrbuf, size_t *buf_len )
{
	_STAINFO            *stainfo = (_STAINFO *)lrbuf->sptr;
	const size_t         offset  = lrbuf->recv_buffer - (uint8_t *)lrbuf;
	PALERTMODE1_HEADER  *pah;
/* */
	int     ret           = 0;
	int     sync_flag     = 0;
	size_t  header_offset = 0;

/* */
	for (
		pah = (PALERTMODE1_HEADER *)lrbuf->recv_buffer;
		*buf_len >= PALERTMODE1_HEADER_LENGTH;
		*buf_len -= PALERTMODE1_HEADER_LENGTH, pah++
	) {
	/* */
		if ( (ret = validate_serial_pah1( pah, stainfo->serial )) > 0 ) {
			if ( ret == PALERTMODE1_PACKET_LENGTH ) {
			/* */
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
		/* */
			if ( header_offset == PALERTMODE1_PACKET_LENGTH ) {
				if ( pa2ew_msgqueue_enqueue( lrbuf, PALERTMODE1_PACKET_LENGTH + offset, RawLogo ) )
					sleep_ew(100);
				header_offset = 0;
			}
		}
	}
/* */
	if ( header_offset )
		*buf_len += header_offset;
	else
		memmove(lrbuf->recv_buffer, pah, *buf_len);

	return sync_flag;
}

/*
 *
 */
static int pre_enqueue_check_pah4( LABELED_RECV_BUFFER *lrbuf, size_t *buf_len )
{
	_STAINFO           *stainfo = (_STAINFO *)lrbuf->sptr;
	const size_t        offset  = lrbuf->recv_buffer - (uint8_t *)lrbuf;
	PALERTMODE4_HEADER *pah4    = (PALERTMODE4_HEADER *)lrbuf->recv_buffer;
/* */
	int ret       = 0;
	int sync_flag = 0;

/* */
	do {
		if ( (ret = validate_serial_pah4( pah4, stainfo->serial )) > 0 ) {
		/* */
			sync_flag = 1;
		/* */
			if ( (uint8_t *)pah4 != lrbuf->recv_buffer ) {
				memmove(lrbuf->recv_buffer, pah4, *buf_len);
				pah4 = (PALERTMODE4_HEADER *)lrbuf->recv_buffer;
			}
		/* */
			if ( *buf_len >= ret ) {
				if ( pa2ew_msgqueue_enqueue( lrbuf, ret + offset, RawLogo ) )
					sleep_ew(100);
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
