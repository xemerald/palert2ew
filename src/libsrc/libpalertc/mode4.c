/**
 * @file mode4.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Program for Palert mode 4 data packet.
 * @version 0.1
 * @date 2024-06-02
 *
 * @copyright Copyright (c) 2024
 *
 */

/* Standard C header include */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* Local header include */
#include "libpalertc.h"
#include "mode4.h"
#include "misc.h"

/**
 * @brief
 *
 * @param pam4h
 * @return char*
 */
char *pac_m4_trigmode_get( const PALERT_M4_HEADER *pam4h )
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
		if ( pam4h->trigger_flag[0] & trigmode_bit[i] )
			return trigmode_str[i];
	}

	return NULL;
}

/**
 * @brief Parse the four kind of Palert IP address
 *
 * @param pam4h
 * @param iptype
 * @param dest
 * @return char*
 */
char *pac_m4_ip_get( const PALERT_M4_HEADER *pam4h, const int iptype, char *dest )
{
	switch ( iptype ) {
	case PALERT_SET_IP:
		misc_ipv4str_gen(dest, pam4h->palert_ip[0], pam4h->palert_ip[1], pam4h->palert_ip[2], pam4h->palert_ip[3]);
		break;
	case PALERT_NTP_IP:
		misc_ipv4str_gen(dest, pam4h->ntp_server[0], pam4h->ntp_server[1], pam4h->ntp_server[2], pam4h->ntp_server[3]);
		break;
	case PALERT_TCP0_IP:
		misc_ipv4str_gen(dest, pam4h->tcp0_server[1], pam4h->tcp0_server[0], pam4h->tcp0_server[3], pam4h->tcp0_server[2]);
		break;
	case PALERT_TCP1_IP:
		misc_ipv4str_gen(dest, pam4h->tcp1_server[1], pam4h->tcp1_server[0], pam4h->tcp1_server[3], pam4h->tcp1_server[2]);
		break;
	case PALERT_TCP2_IP:
		misc_ipv4str_gen(dest, pam4h->tcp2_server[1], pam4h->tcp2_server[0], pam4h->tcp2_server[3], pam4h->tcp2_server[2]);
		break;
	case PALERT_ADMIN0_IP:
		misc_ipv4str_gen(dest, pam4h->admin0_server[1], pam4h->admin0_server[0], pam4h->admin0_server[3], pam4h->admin0_server[2]);
		break;
	case PALERT_ADMIN1_IP:
		misc_ipv4str_gen(dest, pam4h->admin1_server[1], pam4h->admin1_server[0], pam4h->admin1_server[3], pam4h->admin1_server[2]);
		break;
	default:
		fprintf(stderr, "pac_m4_ip_get: Unknown IP type for Palert mode 4 packet.\n");
		break;
	}

	return dest;
}

/**
 * @brief
 *
 * @param packet
 * @return int
 */
int pac_m4_crc_check( const PALERT_M4_PACKET *packet )
{
	return misc_crc16_cal( packet, PALERT_M4_CRC16_CAL_LENGTH ) ? 0 : 1;
}
