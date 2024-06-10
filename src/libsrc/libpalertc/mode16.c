/**
 * @file mode16.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Program for Palert mode 16 data packet.
 * @version 0.1
 * @date 2024-06-02
 *
 * @copyright Copyright (c) 2024
 *
 */

/* Standard C header include */
#include <stdlib.h>
#include <stdint.h>
/* Local header include */
#include "libpalertc.h"
#include "mode16.h"

/**
 * @brief Parse the palert mode 16 timestamp to calendar time(UTC)
 *
 * @param pam16h
 * @return double
 */
double pac_m16_sptime_get( const PALERT_M16_HEADER *pam16h )
{
	return PALERT_M16_TIMESTAMP_GET( pam16h ) + PALERT_M16_WORD_GET( pam16h->msec ) / 10000.0;
}

/**
 * @brief
 *
 * @param pam16h
 * @return double
 */
double pac_m16_scale_get( const PALERT_M16_HEADER *pam16h )
{
	PALERT_M16_DATA data = {
		.data_uint = PALERT_M16_DWORD_GET( pam16h->scale )
	};

	return data.data_real;
}

/**
 * @brief
 *
 * @param pam16h
 * @return double
 */
double pac_m16_ntp_offset_get( const PALERT_M16_HEADER *pam16h )
{
	PALERT_M16_DATA data = {
		.data_uint = PALERT_M16_DWORD_GET( pam16h->ntp_offset )
	};

	return data.data_real;
}

/**
 * @brief
 *
 * @param packet
 * @param nbuf
 * @param buffer
 */
void pac_m16_data_extract( const PALERT_M16_PACKET *packet, int nbuf, float *buffer[] )
{
/* Shortcut for the packet data */
	const PALERT_M16_DATA *       data_ptr = (PALERT_M16_DATA *)&packet->bytes[PALERT_M16_HEADER_LENGTH];
	const PALERT_M16_DATA * const data_end = (PALERT_M16_DATA *)((uint8_t *)data_ptr + PALERT_M16_WORD_GET( packet->header.data_len ));
	PALERT_M16_DATA               data_buf;
/* */
	float  *_buffer[packet->header.nchannel];
	float   dumping[PALERT_MAX_SAMPRATE] = { 0 };

/* */
	for ( int i = 0; i < packet->header.nchannel; i++ )
		_buffer[i] = dumping;
/* */
	for ( int i = 0; nbuf > 0 && i < packet->header.nchannel; nbuf--, i++ )
		if ( buffer[i] )
			_buffer[i] = buffer[i];
/* Go thru all the packet data */
	for ( int i = 0; data_ptr < data_end; i++, data_ptr += packet->header.nchannel ) {
		for ( int j = 0; j < packet->header.nchannel; j++ ) {
			data_buf.data_uint = PALERT_M16_DWORD_GET( data_ptr[j].data_byte );
			_buffer[j][i]      = data_buf.data_real;
		}
	}

	return;
}
