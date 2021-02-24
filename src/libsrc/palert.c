/*
 * palert.c
 *
 * Tool for parsing Palert & PalertPlus data packet.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * May, 2018
 *
 */

/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local header include */
#include <palert.h>

/* Internal functions' prototypes */
static unsigned long _mktime( unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int );
static char *_genipstring( char *, uint8_t, uint8_t, uint8_t, uint8_t );

/*
 * palert_get_systime() - Parse the palert system time to calendar time(UTC)
 */
double palert_get_systime( const PALERTMODE1_HEADER *pah, long tzoffset )
{
#ifdef _SPARC
	return (double)( _mktime(
		((pah->sys_year[1] << 8) + pah->sys_year[0]),
		((pah->sys_month[1] << 8) + pah->sys_month[0]),
		((pah->sys_day[1] << 8) + pah->sys_day[0]),
		((pah->sys_hour[1] << 8) + pah->sys_hour[0]),
		((pah->sys_minute[1] << 8) + pah->sys_minute[0]),
		pah->sys_second ) +
		tzoffset ) +
		((pah->sys_tenmsec << 3) + (pah->sys_tenmsec << 1))/1000.0
	;
#else
	return (double)( _mktime(
		pah->sys_year, pah->sys_month,
		pah->sys_day, pah->sys_hour,
		pah->sys_minute, pah->sys_second ) +
		tzoffset ) +
		((pah->sys_tenmsec << 3) + (pah->sys_tenmsec << 1))/1000.0
	;
#endif
}

/*
 * palert_get_evtime() - Parse the palert event time to calendar time(UTC)
 */
double palert_get_evtime( const PALERTMODE1_HEADER *pah, long tzoffset )
{
#ifdef _SPARC
	return (double)( _mktime(
		((pah->ev_year[1] << 8) + pah->ev_year[0]),
		((pah->ev_month[1] << 8) + pah->ev_month[0]),
		((pah->ev_day[1] << 8) + pah->ev_day[0]),
		((pah->ev_hour[1] << 8) + pah->ev_hour[0]),
		((pah->ev_minute[1] << 8) + pah->ev_minute[0]),
		pah->ev_second ) +
		tzoffset ) +
		((pah->ev_tenmsec << 3) + (pah->ev_tenmsec << 1))/1000.0
	;
#else
	return (double)( _mktime(
		pah->ev_year, pah->ev_month,
		pah->ev_day, pah->ev_hour,
		pah->ev_minute, pah->ev_second ) +
		tzoffset ) +
		((pah->ev_tenmsec << 3) + (pah->ev_tenmsec << 1))/1000.0
	;
#endif
}

/*
 *
 */
char *palert_get_chan_code( const PALERTMODE1_CHANNEL chan_seq )
{
#define X(a, b, c) b,
	static char *chan_code[] = {
		PALERTMODE1_CHAN_TABLE
	};
#undef X

	return chan_code[chan_seq];
}

/*
 *
 */
double palert_get_chan_unit( const PALERTMODE1_CHANNEL chan_seq )
{
#define X(a, b, c) c,
	double chan_unit[] = {
		PALERTMODE1_CHAN_TABLE
	};
#undef X

	return chan_unit[chan_seq];
}

/*
 *
 */
char *palert_get_trigmode_str( const PALERTMODE1_HEADER *pah )
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
	int i;
/* */
	for ( i = 0; i < PALERT_TRIGMODE_COUNT; i++ ) {
		if ( pah->event_flag[0] & trigmode_bit[i] )
			return trigmode_str[i];
	}

	return NULL;
}

/*
 * palert_get_ip() - Parse the four kind of palert IPs
 */
char *palert_get_ip( const PALERTMODE1_HEADER *pah, const int iptype, char *dest )
{
	switch ( iptype ) {
	case PALERT_SET_IP:
		_genipstring(dest, pah->palert_ip[0], pah->palert_ip[1], pah->palert_ip[2], pah->palert_ip[3]);
		break;
	case PALERT_NTP_IP:
		_genipstring(dest, pah->ntp_server[0], pah->ntp_server[1], pah->ntp_server[2], pah->ntp_server[3]);
		break;
	case PALERT_TCP0_IP:
		_genipstring(dest, pah->tcp0_server[1], pah->tcp0_server[0], pah->tcp0_server[3], pah->tcp0_server[2]);
		break;
	case PALERT_TCP1_IP:
		_genipstring(dest, pah->tcp1_server[1], pah->tcp1_server[0], pah->tcp1_server[3], pah->tcp1_server[2]);
		break;
	default:
		printf("palert_get_ip: Unknown IP type for Palert packet.\n");
		break;
	}

	return dest;
}

/*
 *
 */
int32_t *palert_get_data( const PalertPacket *palertp, const int ncmp, int32_t *buffer )
{
	int i;
	const PALERTDATA *_data = palertp->data;

#ifdef _SPARC
	uint32_t hbyte;
	for ( i = 0; i < PALERTMODE1_SAMPLE_NUMBER; i++, palert_data++, buffer++ ) {
		hbyte   = _data->cmp[ncmp][1];
		*buffer = (hbyte << 8) + _data->cmp[ncmp][0];
		if ( hbyte & 0x80 ) *buffer |= 0xffff0000;
	}
#else
	for ( i = 0; i < PALERTMODE1_SAMPLE_NUMBER; i++, _data++, buffer++ )
		*buffer = _data->cmp[ncmp];
#endif

	return buffer;
}

/*
 *
 */
int palert_translate_cwb2020_int( const int raw_intensity )
{
	switch ( raw_intensity ) {
	case 0: default:
		return 0;
	case 10: case 20: case 30: case 40: case 51:
		return raw_intensity / 10;
	case 59:
		return 6;
	case 61:
		return 7;
	case 69:
		return 8;
	case 70:
		return 9;
	}

	return 0;
}

/*
 *
 */
int palert_check_sync_common( const void *header )
{
	PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)header;

	if ( PALERT_IS_MODE1_HEADER(pah) ) {
		return PALERTMODE1_HEADER_CHECK_SYNC(pah);
	}
	else if ( PALERT_IS_MODE4_HEADER(pah) ) {
		PALERTMODE4_HEADER *pah4 = (PALERTMODE4_HEADER *)pah;
		return PALERTMODE4_HEADER_CHECK_SYNC(pah4);
	}

	return 0;
}

/*
 *
 */
int palert_check_ntp_common( const void *header )
{
	PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)header;

	if ( PALERT_IS_MODE1_HEADER(pah) ) {
		return PALERTMODE1_HEADER_CHECK_NTP(pah);
	}
	else if ( PALERT_IS_MODE4_HEADER(pah) ) {
		PALERTMODE4_HEADER *pah4 = (PALERTMODE4_HEADER *)pah;
		return PALERTMODE4_HEADER_CHECK_NTP(pah4);
	}

	return 0;
}

/*
 *
 */
int palert_get_packet_type_common( const void *header )
{
	PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)header;

	if ( PALERT_IS_MODE1_HEADER(pah) )
		return 1;
	else if ( PALERT_IS_MODE4_HEADER(pah) )
		return 4;

	return 0;
}

/*
 *
 */
int palert_get_packet_len_common( const void *header )
{
	PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)header;

	if ( PALERT_IS_MODE1_HEADER(pah) ) {
		return PALERTMODE1_HEADER_GET_PACKETLEN(pah);
	}
	else if ( PALERT_IS_MODE4_HEADER(pah) ) {
		PALERTMODE4_HEADER *pah4 = (PALERTMODE4_HEADER *)pah;
		return PALERTMODE4_HEADER_GET_PACKETLEN(pah4);
	}

	return 0;
}

/*
 *
 */
int palert_get_serial_common( const void *header )
{
	PALERTMODE1_HEADER *pah = (PALERTMODE1_HEADER *)header;

	if ( PALERT_IS_MODE1_HEADER(pah) ) {
		return PALERTMODE1_HEADER_GET_SERIAL(pah);
	}
	else if ( PALERT_IS_MODE4_HEADER(pah) ) {
		PALERTMODE4_HEADER *pah4 = (PALERTMODE4_HEADER *)pah;
		return PALERTMODE4_HEADER_GET_SERIAL(pah4);
	}

	return 0;
}

/*
 * _mktime() - turn the broken time structure into calendar time(UTC)
 */
static unsigned long _mktime(
	unsigned int year, unsigned int mon, unsigned int day,
	unsigned int hour, unsigned int min, unsigned int sec
) {
	if ( 0 >= (int)(mon -= 2) ) {
	/* Puts Feb last since it has leap day */
		mon += 12;
		year--;
	}

	return ((((unsigned long)(year/4 - year/100 + year/400 + 367*mon/12 + day) +
				year*365 - 719499
			)*24 + hour
		)*60 + min
	)*60 + sec;
}

/*
 *
 */
static char *_genipstring(
	char *dest,
	uint8_t first_num, uint8_t second_num, uint8_t third_num, uint8_t fourth_num
) {
	sprintf(dest, "%d.%d.%d.%d", first_num, second_num, third_num, fourth_num);
	return dest;
}
