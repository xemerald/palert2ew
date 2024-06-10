/**
 * @file mode16.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for Palert data mode 16 packet.
 * @version 0.1
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
#define PALERT_M16_HEADER_LENGTH    36

/*
 * Definition of Palert mode 16 header, total size is 36 bytes
 */
typedef struct {
/* Fixed part */
	uint8_t sync_char[4];
	uint8_t packet_no[2];
	uint8_t header_len;
	uint8_t data_len[2];
	uint8_t packet_len[2];
/* Header(Sensor) part */
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
	uint32_t data_uint;
	float    data_real;
} PALERT_M16_DATA;

/*
 * Definition of Palert generic mode 16 packet structure, total size is 65536 bytes
 */
typedef union {
	PALERT_M16_HEADER header;
	uint8_t           bytes[65536];
} PALERT_M16_PACKET;

/* Alias of the structure above */
typedef PALERT_M16_PACKET PAM16P;

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
		(((_PAM16H)->sync_char[0] == 0x53) && ((_PAM16H)->sync_char[1] == 0x59) && \
		((_PAM16H)->sync_char[2] == 0x4F) && ((_PAM16H)->sync_char[3] == 0x43))

/**
 * @brief Parse the palert packet length
 *
 */
#define PALERT_M16_PACKETLEN_GET(_PAM16H) \
		PALERT_M16_WORD_GET((_PAM16H)->packet_len)

/**
 * @brief Parse the palert serial number
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
 * @brief Parse the palert sampling rate
 *
 */
#define PALERT_M16_SAMPRATE_GET(_PAM16H) \
		((PALERT_M16_WORD_GET((_PAM16H)->sps) && PALERT_M16_WORD_GET((_PAM16H)->sps) <= PALERT_MAX_SAMPRATE) ? \
		PALERT_M16_WORD_GET((_PAM16H)->sps) : PALERT_DEFAULT_SAMPRATE)

/**
 * @brief Parse the palert sample number
 *
 */
#define PALERT_M16_SAMPNUM_GET(_PAM16H) \
		((PALERT_M16_WORD_GET((_PAM16H)->data_len) >> 2) / (_PAM16H)->nchannel)

/*
 * PALERT_IS_MODE16_HEADER()
 */
#define PALERT_PKT_IS_MODE16(_PAPKT) \
		((((uint8_t *)(_PAPKT))[0] == 0x53) && (((uint8_t *)(_PAPKT))[1] == 0x59) && \
		(((uint8_t *)(_PAPKT))[2] == 0x4F) && (((uint8_t *)(_PAPKT))[3] == 0x43))

/* Export functions's prototypes */
double pac_m16_sptime_get( const PALERT_M16_HEADER * );
double pac_m16_scale_get( const PALERT_M16_HEADER * );
double pac_m16_ntp_offset_get( const PALERT_M16_HEADER * );
void   pac_m16_data_extract( const PALERT_M16_PACKET *, int, float *[] );
