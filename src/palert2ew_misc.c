/*
 *
 */
/* Standard C header include */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
/* Earthworm environment header include */
#include <trace_buf.h>
/* */
#include <palert2ew.h>

/* */
static uint8_t cal_crc8_high( const uint8_t );
/* */
static uint8_t CRC8_Table[256] = { 0 };
static uint8_t CRC8_Ready = 0;

/*
 * pa2ew_misc_trh2_enrich() -
 */
TRACE2_HEADER *pa2ew_misc_trh2_enrich(
	TRACE2_HEADER *dest, const char *sta, const char *net, const char *loc,
	const int nsamp, const double samprate, const double starttime
) {
/* */
	dest->pinno     = 0;
	dest->nsamp     = nsamp;
	dest->samprate  = samprate;
	dest->starttime = starttime;
	dest->endtime   = dest->starttime + (dest->nsamp - 1) / dest->samprate;
/* */
	memcpy(dest->sta, sta, TRACE2_STA_LEN);
	memcpy(dest->net, net, TRACE2_NET_LEN);
	memcpy(dest->loc, loc, TRACE2_LOC_LEN);
/* */
	dest->version[0] = TRACE2_VERSION0;
	dest->version[1] = TRACE2_VERSION1;

	strcpy(dest->quality, TRACE2_NO_QUALITY);
	strcpy(dest->pad    , TRACE2_NO_PAD    );

	dest->datatype[1] = '4';
	dest->datatype[2] = '\0';
#if defined( _SPARC )
	dest->datatype[0] = 's';   /* SUN IEEE integer       */
#elif defined( _INTEL )
	dest->datatype[0] = 'i';   /* VAX/Intel IEEE integer */
#else
	fprintf(stderr, "palert2ew: warning _INTEL and _SPARC are both undefined.");
#endif

	return dest;
}

/*
 *
 */
double pa2ew_misc_timenow_get( void )
{
	struct timespec time_sp;
	double          result = 0.0;

/* */
	clock_gettime(CLOCK_REALTIME_COARSE, &time_sp);
	result = (double)time_sp.tv_sec + (double)time_sp.tv_nsec * 1.0e-9;

	return result;
}

/*
 *
 */
int pa2ew_misc_recv_thrdnum_eval( int max_stations, const int server_switch )
{
	int result;
/* */
	if ( server_switch ) {
		for ( result = 0; max_stations > 0; max_stations -= PA2EW_MAX_PALERTS_PER_THREAD )
			result++;
	}
	else {
		result = 1;
	}

	return result;
}

/*
 * pa2ew_misc_crc8_init() - A CRC-8 initialization function
 */
void pa2ew_misc_crc8_init( void )
{
	int i;

/* */
	for ( i = 0x00; i <= 0xff; i++ )
		CRC8_Table[i & 0xff] = cal_crc8_high( i & 0xff );
/* */
	CRC8_Ready = 1;

	return;
}

/*
 * pa2ew_misc_crc8_cal() - A CRC-8 calculation function
 */
uint8_t pa2ew_misc_crc8_cal( const void *data, const size_t size )
{
	const uint8_t *ptr = data;
	const uint8_t *end;
	uint8_t        result = PA2EW_RECV_SERVER_CRC8_INIT;

/* */
	if ( !CRC8_Ready )
		pa2ew_misc_crc8_init();
/* */
	if ( ptr ) {
	/* */
		end = ptr + size;
		while ( ptr < end )
			result = CRC8_Table[result ^ *ptr++];
	}

	return result;
}

/*
 *
 */
static uint8_t cal_crc8_high( const uint8_t data )
{
	uint8_t result = PA2EW_RECV_SERVER_CRC8_INIT;
	int     i;

/* */
	result ^= data;
	for ( i = 0; i < 8; i++ ) {
		if ( result & 0x80 )
			result = (result << 1) ^ PA2EW_RECV_SERVER_CRC8_POLY;
		else
			result <<= 1;
	}

	return result;
}
