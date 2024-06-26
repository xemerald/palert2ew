/**
 * @file mode16.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for Palert data mode 16 packet.
 * @date 2024-06-02
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
/* */
#include <stdint.h>
/* */
#include "samprate.h"

/* */
#define PALERT_M16_HEADER_LENGTH      36
#define PALERT_M16_PACKET_MAX_LENGTH  65536
/* */
#define PALERT_M16_COUNT_OVER_GAL     2924

/**
 * @brief Definition of Palert mode 16 header, total size is 36 bytes
 *
 */
typedef struct {
/* Fixed part */
	uint8_t sync_char[4];
	uint8_t packet_no[2];
	uint8_t header_len;
	uint8_t data_len[2];
	uint8_t packet_len[2];
/* Header(Sensor information) part */
	uint8_t unixtime[5];
	uint8_t msec[2];
	uint8_t ntp_sync;
	uint8_t scale[4];
	uint8_t sps[2];
	uint8_t sensor_type;
	uint8_t nchannel;
	uint8_t serial[4];
	uint8_t ntp_offset[4];
	uint8_t dummy;
} PALERT_M16_HEADER;

/* Alias of the structure above */
typedef PALERT_M16_HEADER PAM16H;

/**
 * @brief
 *
 */
typedef union {
	uint8_t  data_byte[4];
	uint32_t data_dword;
	float    data_real;
} PALERT_M16_DATA;

/**
 * @brief Definition of Palert generic mode 16 packet structure, total size is 65536 bytes
 *
 */
typedef union {
	PALERT_M16_HEADER header;
	uint8_t           bytes[PALERT_M16_PACKET_MAX_LENGTH];
} PALERT_M16_PACKET;

/* Alias of the structure above */
typedef PALERT_M16_PACKET PAM16P;

/**
 * @brief
 *
 */
#define PALERT_M16_SYNC_CHAR_0  0x53  /* Equal to ASCII 'S' */
#define PALERT_M16_SYNC_CHAR_1  0x59  /* Equal to ASCII 'Y' */
#define PALERT_M16_SYNC_CHAR_2  0x4E  /* Equal to ASCII 'N' */
#define PALERT_M16_SYNC_CHAR_3  0x43  /* Equal to ASCII 'C' */

/**
 * @brief
 *
 */
#define PALERT_M16_WORD_GET(_PAM16H_WORD) \
		(((uint16_t)((_PAM16H_WORD)[1]) << 8) | (uint16_t)((_PAM16H_WORD)[0]))

/**
 * @brief
 *
 */
#define PALERT_M16_DWORD_GET(_PAM16H_DWORD) \
		(((uint32_t)((_PAM16H_DWORD)[3]) << 24) | ((uint32_t)((_PAM16H_DWORD)[2]) << 16) | ((uint32_t)((_PAM16H_DWORD)[1]) << 8) | (uint32_t)((_PAM16H_DWORD)[0]))

/**
 * @brief
 *
 */
#define PALERT_M16_NTP_CHECK(_PAM16H) \
		((_PAM16H)->ntp_sync & 0x01)

/**
 * @brief
 *
 */
#define PALERT_M16_SYNC_CHECK(_PAM16H) \
		(((_PAM16H)->sync_char[0] == PALERT_M16_SYNC_CHAR_0) && ((_PAM16H)->sync_char[1] == PALERT_M16_SYNC_CHAR_1) && \
		((_PAM16H)->sync_char[2] == PALERT_M16_SYNC_CHAR_2) && ((_PAM16H)->sync_char[3] == PALERT_M16_SYNC_CHAR_3))

/**
 * @brief Parse the Palert mode 16 packet length
 *
 */
#define PALERT_M16_PACKETLEN_GET(_PAM16H) \
		PALERT_M16_WORD_GET((_PAM16H)->packet_len)

/**
 * @brief Parse the Palert mode 16 serial number
 *
 */
#define PALERT_M16_SERIAL_GET(_PAM16H) \
		PALERT_M16_DWORD_GET((_PAM16H)->serial)

/**
 * @brief
 *
 */
#define PALERT_M16_TIMESTAMP_GET(_PAM16H) \
		(((uint64_t)((_PAM16H)->unixtime[4]) << 32) | \
		(((uint32_t)((_PAM16H)->unixtime[3]) << 24) | ((uint32_t)((_PAM16H)->unixtime[2]) << 16) | ((uint32_t)((_PAM16H)->unixtime[1]) << 8) | (uint32_t)((_PAM16H)->unixtime[0])))

/**
 * @brief Parse the Palert mode 16 sampling rate
 *
 */
#define PALERT_M16_SAMPRATE_GET(_PAM16H) \
		PALERT_M16_WORD_GET((_PAM16H)->sps)
/**
 * @brief Parse the Palert mode 16 sample number
 *
 */
#define PALERT_M16_SAMPNUM_GET(_PAM16H) \
		((PALERT_M16_WORD_GET((_PAM16H)->data_len) >> 2) / (_PAM16H)->nchannel)

/**
 * @brief
 *
 */
#define PALERT_PKT_IS_MODE16(_PAPKT) \
		((((uint8_t *)(_PAPKT))[0] == PALERT_M16_SYNC_CHAR_0) && (((uint8_t *)(_PAPKT))[1] == PALERT_M16_SYNC_CHAR_1) && \
		(((uint8_t *)(_PAPKT))[2] == PALERT_M16_SYNC_CHAR_2) && (((uint8_t *)(_PAPKT))[3] == PALERT_M16_SYNC_CHAR_3))

/* Export functions's prototypes */
double pac_m16_sptime_get( const PALERT_M16_HEADER * );
double pac_m16_scale_get( const PALERT_M16_HEADER * );
double pac_m16_ntp_offset_get( const PALERT_M16_HEADER * );
void   pac_m16_data_extract( const PALERT_M16_PACKET *, int, float *[] );
void   pac_m16_idata_extract( const PALERT_M16_PACKET *, int, int32_t *[] );
int    pac_m16_crc_check( const PALERT_M16_PACKET * );
