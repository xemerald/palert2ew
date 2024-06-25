/**
 * @file general.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief
 * @date 2024-06-04
 *
 * @copyright Copyright (c) 2024
 *
 */
/* Local header include */
#include "libpalertc.h"
#include "mode1.h"
#include "mode4.h"
#include "mode16.h"

/**
 * @brief
 *
 * @param packet
 * @return int
 */
int pac_mode_get( const void *packet )
{
	if ( PALERT_PKT_IS_MODE16( packet ) )
		return PALERT_PKT_MODE16;
	else if ( PALERT_PKT_IS_MODE4( packet ) )
		return PALERT_PKT_MODE4;
	else if ( PALERT_PKT_IS_MODE2( packet ) )
		return PALERT_PKT_MODE2;
	else if ( PALERT_PKT_IS_MODE1( packet ) )
		return PALERT_PKT_MODE1;

	return 0;
}

/**
 * @brief
 *
 * @param packet
 * @return int
 */
int pac_sync_check( const void *packet )
{
	if ( PALERT_PKT_IS_MODE16( packet ) )
		return PALERT_M16_SYNC_CHECK( (PALERT_M16_HEADER *)packet );
	else if ( PALERT_PKT_IS_MODE4( packet ) )
		return PALERT_M4_SYNC_CHECK( (PALERT_M4_HEADER *)packet );
	else
		return PALERT_M1_SYNC_CHECK( (PALERT_M1_HEADER *)packet );

	return 0;
}

/**
 * @brief
 *
 * @param packet
 * @return int
 */
int pac_ntp_sync_check( const void *packet )
{
	if ( PALERT_PKT_IS_MODE16( packet ) )
		return PALERT_M16_NTP_CHECK( (PALERT_M16_HEADER *)packet );
	else if ( PALERT_PKT_IS_MODE4( packet ) )
		return PALERT_M4_NTP_CHECK( (PALERT_M4_HEADER *)packet );
	else
		return PALERT_M1_NTP_CHECK( (PALERT_M1_HEADER *)packet );

	return 0;
}

/**
 * @brief
 *
 * @param packet
 * @return int
 */
int pac_pktlen_get( const void *packet )
{
	if ( PALERT_PKT_IS_MODE16( packet ) )
		return PALERT_M16_PACKETLEN_GET( (PALERT_M16_HEADER *)packet );
	else if ( PALERT_PKT_IS_MODE4( packet ) )
		return PALERT_M4_PACKETLEN_GET( (PALERT_M4_HEADER *)packet );
	else
		return PALERT_M1_PACKETLEN_GET( (PALERT_M1_HEADER *)packet );

	return 0;
}

/**
 * @brief
 *
 * @param packet
 * @return int
 */
int pac_serial_get( const void *packet )
{
	if ( PALERT_PKT_IS_MODE16( packet ) )
		return PALERT_M16_SERIAL_GET( (PALERT_M16_HEADER *)packet );
	else if ( PALERT_PKT_IS_MODE4( packet ) )
		return PALERT_M4_SERIAL_GET( (PALERT_M4_HEADER *)packet );
	else
		return PALERT_M1_SERIAL_GET( (PALERT_M1_HEADER *)packet );

	return 0;
}

/**
 * @brief
 *
 * @param raw_intensity
 * @return int
 */
int pac_cwb2020_int_trans( const int raw_intensity )
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
