/**
 * @file mode1.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief
 * @version 0.1
 * @date 2024-06-02
 *
 * @copyright Copyright (c) 2024
 *
 */

/* Standard C header include */
#include <stdio.h>
#include <stdint.h>
/* Local header include */
#include "libpalertc.h"
#include "mode1.h"
#include "misc.h"

/**
 * @brief Parse the Palert Mode 1 system time to calendar time(UTC)
 *
 * @param pam1h
 * @param tzoffset_sec
 * @return double
 */
double pac_m1_systime_get( const PALERT_M1_HEADER *pam1h, long tzoffset_sec )
{
	return
		misc_mktime(
			PALERT_M1_WORD_GET( pam1h->sys_year ),
			PALERT_M1_WORD_GET( pam1h->sys_month ),
			PALERT_M1_WORD_GET( pam1h->sys_day ),
			PALERT_M1_WORD_GET( pam1h->sys_hour ),
			PALERT_M1_WORD_GET( pam1h->sys_minute ),
			pam1h->sys_second
		) + tzoffset_sec + ((pam1h->sys_tenmsec << 3) + (pam1h->sys_tenmsec << 1)) / 1000.0;
}

/**
 * @brief Parse the Palert Mode 1 event time to calendar time(UTC)
 *
 * @param pam1h
 * @param tzoffset_sec
 * @return double
 */
double pac_m1_evtime_get( const PALERT_M1_HEADER *pam1h, long tzoffset_sec )
{
	return
		misc_mktime(
			PALERT_M1_WORD_GET( pam1h->ev_year ),
			PALERT_M1_WORD_GET( pam1h->ev_month ),
			PALERT_M1_WORD_GET( pam1h->ev_day ),
			PALERT_M1_WORD_GET( pam1h->ev_hour ),
			PALERT_M1_WORD_GET( pam1h->ev_minute ),
			pam1h->ev_second
		) + tzoffset_sec + ((pam1h->ev_tenmsec << 3) + (pam1h->ev_tenmsec << 1)) / 1000.0;
}

/**
 * @brief
 *
 * @param chan_seq
 * @return char*
 */
char *pac_m1_chan_code_get( const int chan_seq )
{
#define X(a, b, c) b,
	static char *chan_code[] = {
		PALERT_M1_CHAN_TABLE
	};
#undef X

	return chan_code[chan_seq];
}

/**
 * @brief
 *
 * @param chan_seq
 * @return double
 */
double pac_m1_chan_unit_get( const int chan_seq )
{
#define X(a, b, c) c,
	double chan_unit[] = {
		PALERT_M1_CHAN_TABLE
	};
#undef X

	return chan_unit[chan_seq];
}

/**
 * @brief
 *
 * @param pam1h
 * @return char*
 */
char *pac_m1_trigmode_get( const PALERT_M1_HEADER *pam1h )
{
/* */
#define X(a, b, c) b,
	static char *trigmode_str[] = {
		PALERT_TRIGMODE_TABLE
	};
#undef X
#define X(a, b, c) c,
	uint8_t trigmode_bit[] = {
		PALERT_TRIGMODE_TABLE
	};
#undef X
/* */
	for ( int i = 0; i < PALERT_TRIGMODE_COUNT; i++ ) {
		if ( pam1h->event_flag[0] & trigmode_bit[i] )
			return trigmode_str[i];
	}

	return NULL;
}

/**
 * @brief Parse the four kind of Palert IP address
 *
 * @param pam1h
 * @param iptype
 * @param dest
 * @return char*
 */
char *pac_m1_ip_get( const PALERT_M1_HEADER *pam1h, const int iptype, char *dest )
{
	switch ( iptype ) {
	case PALERT_SET_IP:
		misc_ipv4str_gen(dest, pam1h->palert_ip[0], pam1h->palert_ip[1], pam1h->palert_ip[2], pam1h->palert_ip[3]);
		break;
	case PALERT_NTP_IP:
		misc_ipv4str_gen(dest, pam1h->ntp_server[0], pam1h->ntp_server[1], pam1h->ntp_server[2], pam1h->ntp_server[3]);
		break;
	case PALERT_TCP0_IP:
		misc_ipv4str_gen(dest, pam1h->tcp0_server[1], pam1h->tcp0_server[0], pam1h->tcp0_server[3], pam1h->tcp0_server[2]);
		break;
	case PALERT_TCP1_IP:
		misc_ipv4str_gen(dest, pam1h->tcp1_server[1], pam1h->tcp1_server[0], pam1h->tcp1_server[3], pam1h->tcp1_server[2]);
		break;
	default:
		fprintf(stderr, "pac_m1_ip_get: Unknown IP type for Palert mode 1 or 2 packet.\n");
		break;
	}

	return dest;
}

/**
 * @brief
 *
 * @param packet
 * @param buffer
 */
void pac_m1_data_extract( const PALERT_M1_PACKET *packet, int32_t *buffer[PALERT_M1_CHAN_COUNT] )
{
/* Shortcut for the packet data */
	const PALERT_M1_DATA *_data = packet->data;
/* */
	int32_t  dumping[PALERT_M1_SAMPLE_NUMBER];  /* Zero init. is unnecessary */
	uint16_t word;

/* */
	for ( int i = 0; i < PALERT_M1_CHAN_COUNT; i++ )
		if ( !buffer[i] )
			buffer[i] = dumping;

/* Go thru all the data */
	for ( int i = 0; i < PALERT_M1_SAMPLE_NUMBER; i++, _data++ ) {
	/* Channel HLZ(0) */
		word = ((uint16_t)_data->cmp[0][1] << 8) | _data->cmp[0][0];
		buffer[0][i] = *(int16_t *)&word;
	/* Channel HLN(1) */
		word = ((uint16_t)_data->cmp[1][1] << 8) | _data->cmp[1][0];
		buffer[1][i] = *(int16_t *)&word;
	/* Channel HLE(2) */
		word = ((uint16_t)_data->cmp[2][1] << 8) | _data->cmp[2][0];
		buffer[2][i] = *(int16_t *)&word;
	/* Channel PD(3) */
		word = ((uint16_t)_data->cmp[3][1] << 8) | _data->cmp[3][0];
		buffer[3][i] = *(int16_t *)&word;
	/* Channel Disp(4) */
		word = ((uint16_t)_data->cmp[4][1] << 8) | _data->cmp[4][0];
		buffer[4][i] = *(int16_t *)&word;
	}

	return;
}


/**
 * @brief
 *
 * @param packet
 * @return int
 */
int pac_m1_crc_check( const PALERT_M1_PACKET *packet )
{
/* */
	PALERT_M1_HEADER *header = (PALERT_M1_HEADER *)&packet->header;
	const uint8_t     crc16_byte[2] = { header->crc16_byte[0], header->crc16_byte[1] };
	int               result;

/* */
	header->crc16_byte[0] = header->crc16_byte[1] = 0x00;
	result = misc_crc16_cal( packet, PALERT_M1_PACKETLEN_GET( header ) );
	result = result == (crc16_byte[0] | (crc16_byte[1] << 8)) ? 1 : 0;
/* */
	header->crc16_byte[0] = crc16_byte[0];
	header->crc16_byte[1] = crc16_byte[1];

	return result;
}
