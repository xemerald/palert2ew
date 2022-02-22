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

/* */
#define RATIO_LBUF_SIZE_STATIONS  0.02
#define MINIMUM_LBUF_SIZE         2

/* Internal stack related struct */
struct last_buffer {
	uint16_t serial;
	uint16_t buffer_rear;
	uint8_t  buffer[PA2EW_RECV_BUFFER_LENGTH];
};

/* Define global variables */
static mutex_t  QueueMutex;
static QUEUE    MsgQueue;         /* from queue.h, queue.c; sets up linked */
/* */
static struct last_buffer *LastBuffer = NULL;
static volatile int        LastBufferSize = 0;
static volatile int        LastBufferInuse = 0;
static mutex_t             LastBufferMutex;

/* */
static void                 save_last_buffer( void *, const size_t );
static LABELED_RECV_BUFFER *draw_last_buffer( void *, size_t * );
static int pre_enqueue_check_pah1( LABELED_RECV_BUFFER *, size_t *, MSG_LOGO );
static int pre_enqueue_check_pah4( LABELED_RECV_BUFFER *, size_t *, MSG_LOGO );
static int validate_serial_pah1( const PALERTMODE1_HEADER *, const int );
static int validate_serial_pah4( const PALERTMODE4_HEADER *, const int );
/* */
static struct last_buffer *init_last_buffers( const int );
static struct last_buffer *adjust_last_buffers( const int );
static struct last_buffer *sort_last_buffers( void );
static struct last_buffer *reg_last_buffer( const uint16_t );
static struct last_buffer *find_last_buffer( const uint16_t );
static int compare_serial( const void *, const void * );

/*
 * pa2ew_msgqueue_init() - Initialization function of message queue and mutex.
 */
int pa2ew_msgqueue_init( const unsigned long queue_size, const unsigned long element_size )
{
/* Create a Mutex to control access to queue */
	CreateSpecificMutex(&QueueMutex);
/* Initialize the message queue */
	initqueue( &MsgQueue, queue_size, element_size + 1 );
/* */
	init_last_buffers( pa2ew_list_total_station_get() );

	return 0;
}

/*
 * pa2ew_msgqueue_end() - End process of message queue.
 */
void pa2ew_msgqueue_end( void )
{
	RequestSpecificMutex(&LastBufferMutex);
	free(LastBuffer);
	ReleaseSpecificMutex(&LastBufferMutex);
	CloseSpecificMutex(&LastBufferMutex);
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
int pa2ew_msgqueue_rawpacket( void *label_buf, size_t buf_len, int packet_type, MSG_LOGO logo )
{
	LABELED_RECV_BUFFER *lrbuf;
	int                  sync_flag = 0;

/* */
	buf_len -= lrbuf->recv_buffer - (uint8_t *)lrbuf;
	RequestSpecificMutex(&LastBufferMutex);
	lrbuf = draw_last_buffer( label_buf, &buf_len );
	ReleaseSpecificMutex(&LastBufferMutex);

/* */
	if ( packet_type == 1 )
		sync_flag = pre_enqueue_check_pah1( lrbuf, &buf_len, logo );
	else if ( packet_type == 4 )
		sync_flag = pre_enqueue_check_pah4( lrbuf, &buf_len, logo );
	else
		buf_len = 0;
/* Try to store the remainder data into the buffer */
	if ( buf_len ) {
		RequestSpecificMutex(&LastBufferMutex);
		save_last_buffer( lrbuf, buf_len );
		ReleaseSpecificMutex(&LastBufferMutex);
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
static void save_last_buffer( void *label_buf, const size_t buf_len )
{
	LABELED_RECV_BUFFER *lrbuf   = (LABELED_RECV_BUFFER *)label_buf;
	struct last_buffer *_lastbuf = find_last_buffer( lrbuf->label.serial );
/* */
	if ( !_lastbuf )
		_lastbuf = reg_last_buffer( lrbuf->label.serial );
/* */
	if ( buf_len < PA2EW_RECV_BUFFER_LENGTH ) {
		_lastbuf->buffer_rear = buf_len;
		memcpy(_lastbuf->buffer, lrbuf->recv_buffer, buf_len);
	}
	sort_last_buffers();

	return;
}

/*
 *
 */
static LABELED_RECV_BUFFER *draw_last_buffer( void *label_buf, size_t *buf_len )
{
	LABELED_RECV_BUFFER *result   = (LABELED_RECV_BUFFER *)label_buf;
	struct last_buffer  *_lastbuf = find_last_buffer( result->label.serial );

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
				(LABELED_RECV_BUFFER *)malloc(sizeof(LABELED_RECV_BUFFER) + _lastbuf->buffer_rear);

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
		_lastbuf->serial = 0;
		sort_last_buffers();
		LastBufferInuse--;
	}

	return result;
}

/*
 *
 */
static int pre_enqueue_check_pah1( LABELED_RECV_BUFFER *lrbuf, size_t *buf_len, MSG_LOGO logo )
{
	uint16_t            serial = lrbuf->label.serial;
	const size_t        offset = lrbuf->recv_buffer - (uint8_t *)lrbuf;
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
	/* */
		if ( (ret = validate_serial_pah1( pah, serial )) > 0 ) {
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
				if ( pa2ew_msgqueue_enqueue( lrbuf, PALERTMODE1_PACKET_LENGTH + offset, logo ) )
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
static int pre_enqueue_check_pah4( LABELED_RECV_BUFFER *lrbuf, size_t *buf_len, MSG_LOGO logo )
{
	uint16_t            serial = lrbuf->label.serial;
	const size_t        offset = lrbuf->recv_buffer - (uint8_t *)lrbuf;
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
				if ( pa2ew_msgqueue_enqueue( lrbuf, ret + offset, logo ) )
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

/*
 *
 */
static struct last_buffer *init_last_buffers( const int station_num )
{
	int _num = station_num * RATIO_LBUF_SIZE_STATIONS;

	LastBufferSize = _num ? _num : MINIMUM_LBUF_SIZE;
	LastBuffer     = calloc(LastBufferSize, sizeof(struct last_buffer));
	CreateSpecificMutex(&LastBufferMutex);

	if ( !LastBuffer ) {
		logit("e", "palert2ew: Last buffer cannot allocate memory, exit!\n");
		exit(-1);
	}

	return LastBuffer;
}

/*
 *
 */
static struct last_buffer *adjust_last_buffers( const int station_num )
{
	int _num = station_num * RATIO_LBUF_SIZE_STATIONS;

	if ( LastBufferSize > _num ) {
		if ( LastBufferInuse < _num || LastBufferInuse < MINIMUM_LBUF_SIZE ) {
			LastBufferSize = _num ? _num : MINIMUM_LBUF_SIZE;
			LastBuffer = realloc(LastBuffer, LastBufferSize * sizeof(struct last_buffer));
		}
	}

	return LastBuffer;
}

/*
 *
 */
static struct last_buffer *sort_last_buffers( void )
{
	qsort(LastBuffer, LastBufferSize, sizeof(struct last_buffer), compare_serial);

	return LastBuffer;
}

/*
 *
 */
static struct last_buffer *reg_last_buffer( const uint16_t serial )
{
	int i;
	struct last_buffer *result = NULL;

	for ( i = 0; i < LastBufferSize; i++ ) {
		if ( LastBuffer[i].serial == 0 ) {
			result = LastBuffer + i;
			break;
		}
	}
/* */
	if ( !result ) {
		logit("o", "palert2ew: Last buffer not enough, allocating more memory.\n");
		LastBufferSize++;
		LastBuffer = realloc(LastBuffer, LastBufferSize * sizeof(struct last_buffer));
		result = LastBuffer + LastBufferSize;
	}
/* */
	result->serial = serial;
	result->buffer_rear = 0;
	LastBufferInuse++;

	return result;
}

/*
 *
 */
static struct last_buffer *find_last_buffer( const uint16_t serial )
{
	uint16_t _key;

/* */
	_key = serial;

	return (struct last_buffer *)bsearch(&_key, LastBuffer, LastBufferSize, sizeof(struct last_buffer), compare_serial);
}

/*
 * compare_serial() - reverse version of compare function.
 */
static int compare_serial( const void *node_a, const void *node_b )
{
	int serial_a = ((struct last_buffer *)node_a)->serial;
	int serial_b = ((struct last_buffer *)node_b)->serial;

	if ( serial_a > serial_b )
		return -1;
	else if ( serial_a < serial_b )
		return 1;
	else
		return 0;
}
