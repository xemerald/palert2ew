/**
 * @file palert2ew_misc.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */

/**
 * @name Standard C header include
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/**
 * @name Earthworm environment header include
 *
 */
#include <trace_buf.h>

/**
 * @name Local header include
 *
 */
#include <palert2ew.h>

/**
 * @name Internal functions' prototype
 *
 */
static uint8_t cal_crc8_high( const uint8_t );

/**
 * @name Internal static variables
 *
 */
static uint8_t CRC8_Table[256] = { 0 };
static uint8_t CRC8_Ready = 0;

/**
 * @brief
 *
 * @param dest
 * @return TRACE2_HEADER*
 */
TRACE2_HEADER *pa2ew_trh2_init( TRACE2_HEADER *dest )
{
/* */
	dest->pinno = 0;
/* */
	dest->version[0] = TRACE2_VERSION0;
	dest->version[1] = TRACE2_VERSION1;
/* */
	memset(dest->quality, 0, sizeof(dest->quality));
	memset(dest->pad    , 0, sizeof(dest->pad)    );

	return dest;
}

/**
 * @brief
 *
 * @param dest
 * @param sta
 * @param net
 * @param loc
 * @param chan
 * @return TRACE2_HEADER*
 */
TRACE2_HEADER *pa2ew_trh2_scn_enrich( TRACE2_HEADER *dest, const char *sta, const char *net, const char *loc )
{
/* */
	memcpy(dest->sta, sta, TRACE2_STA_LEN);
	memcpy(dest->net, net, TRACE2_NET_LEN);
	memcpy(dest->loc, loc, TRACE2_LOC_LEN);

	return dest;
}

/**
 * @brief
 *
 * @param dest
 * @param nsamp
 * @param samprate
 * @param starttime
 * @param datatype
 * @return TRACE2_HEADER*
 */
TRACE2_HEADER *pa2ew_trh2_sampinfo_enrich(
	TRACE2_HEADER *dest, const int nsamp, const double samprate, const double starttime, const char datatype[2]
) {
/* */
	dest->nsamp     = nsamp;
	dest->samprate  = samprate;
	dest->starttime = starttime;
	dest->endtime   = dest->starttime + (dest->nsamp - 1) / dest->samprate;
/* */
	dest->datatype[0] = datatype[0];
	dest->datatype[1] = datatype[1];
	dest->datatype[2] = '\0';

	return dest;
}


/**
 * @brief
 *
 * @return double
 */
double pa2ew_timenow_get( void )
{
	struct timespec time_sp;
	double          result = 0.0;

/* */
	clock_gettime(CLOCK_REALTIME_COARSE, &time_sp);
	result = (double)time_sp.tv_sec + (double)time_sp.tv_nsec * 1.0e-9;

	return result;
}

/**
 * @brief
 *
 * @param max_stations
 * @param server_switch
 * @return int
 */
int pa2ew_recv_thrdnum_eval( int max_stations, const int server_switch )
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

/**
 * @brief
 *
 * @return int
 */
int pa2ew_endian_get( void )
{
	uint16_t probe = 1;
/* */
	return *(uint8_t *)&probe ? PA2EW_LITTLE_ENDIAN : PA2EW_BIG_ENDIAN;
}

/**
 * @brief A CRC-8 initialization function
 *
 */
void pa2ew_crc8_init( void )
{
/* */
	for ( int i = 0x00; i <= 0xff; i++ )
		CRC8_Table[i & 0xff] = cal_crc8_high( i & 0xff );
/* */
	CRC8_Ready = 1;

	return;
}

/**
 * @brief A CRC-8 calculation function
 *
 * @param data
 * @param size
 * @return uint8_t
 */
uint8_t pa2ew_crc8_cal( const void *data, const size_t size )
{
	const uint8_t *ptr = data;
	const uint8_t *end;
	uint8_t        result = PA2EW_RECV_SERVER_CRC8_INIT;

/* */
	if ( !CRC8_Ready )
		pa2ew_crc8_init();
/* */
	if ( ptr ) {
	/* */
		end = ptr + size;
		while ( ptr < end )
			result = CRC8_Table[result ^ *ptr++];
	}

	return result;
}

/**
 * @brief A real CRC-8 calculation function
 *
 * @param data
 * @return uint8_t
 */
static uint8_t cal_crc8_high( const uint8_t data )
{
	uint8_t result = PA2EW_RECV_SERVER_CRC8_INIT;

/* */
	result ^= data;
	for ( int i = 0; i < 8; i++ ) {
		if ( result & 0x80 )
			result = (result << 1) ^ PA2EW_RECV_SERVER_CRC8_POLY;
		else
			result <<= 1;
	}

	return result;
}
