/**
 * @file mode4.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for Palert mode 4 data packet.
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
#define PALERT_M4_HEADER_LENGTH    64

/*
 * Definition of Palert mode 4 header, total size is 64 bytes
 */
typedef struct {
/* packet infomation */
	uint8_t packet_type[2];
	uint8_t packet_len[2];
	uint8_t device_type;
	uint8_t channel_number;
	uint8_t crc16_byte[2];
/* hardware infomation */
	uint8_t firmware[2];
	uint8_t serial[2];
	uint8_t connection_flag[2];
	uint8_t trigger_flag[2];
	uint8_t op_mode[2];
	uint8_t dio_status[2];
	uint8_t filter_trigger_mode[2];
/* network infomation */
	uint8_t ntp_server[4];
	uint8_t tcp0_server[4];
	uint8_t tcp1_server[4];
	uint8_t tcp2_server[4];
	uint8_t admin0_server[4];
	uint8_t admin1_server[4];
	uint8_t palert_ip[4];
	uint8_t subnet_mask[4];
	uint8_t gateway_ip[4];
/* synchronized character */
	uint8_t sync_char[4];
/* reserved byte */
	uint8_t padding[2];
} PALERT_M4_HEADER;

/* Alias of the structure above */
typedef PALERT_M4_HEADER PAM4H;

/*
 * Definition of Streamline mini-SEED data record structure
 */
/*
typedef struct {
	struct fsdh_s fsdh;
	uint16_t blkt_type;
	uint16_t next_blkt;
	struct blkt_1000_s blkt1000;
	uint8_t smsrlength[2];
	uint8_t reserved[6];
} SMSRECORD;
*/

/**
 * @brief
 *
 */
#define PALERT_M4_WORD_GET(_PAM4H_WORD) \
		(((uint16_t)((_PAM4H_WORD)[1]) << 8) | (uint16_t)((_PAM4H_WORD)[0]))

/**
 * @brief
 *
 */
#define PALERT_M4_SYNC_CHECK(_PAM4H) \
		((_PAM4H)->sync_char[0] == 0x03 && (_PAM4H)->sync_char[1] == 0x05 && \
		(_PAM4H)->sync_char[2] == 0x15 && (_PAM4H)->sync_char[3] == 0x01)

/**
 * @brief
 *
 */
#define PALERT_M4_NTP_CHECK(_PAM4H) \
		((_PAM4H)->connection_flag[0] & 0x01)

/**
 * @brief
 *
 */
#define PALERT_M4_PACKETLEN_GET(_PAM4H) \
		PALERT_M4_WORD_GET((_PAM4H)->packet_len)

/**
 * @brief
 *
 */
#define PALERT_M4_SERIAL_GET(_PAM4H) \
		PALERT_M4_WORD_GET((_PAM4H)->serial)

/**
 * @brief Parse the palert firmware version
 *
 */
#define PALERT_M4_FIRMWARE_GET(_PAM4H) \
		PALERT_M4_WORD_GET((_PAM4H)->firmware)

/**
 * @brief Parse the palert packet type
 *
 */
#define PALERT_M4_PACKETTYPE_GET(_PAM4H) \
		PALERT_M4_WORD_GET((_PAM4H)->packet_type)
/**
 * @brief
 *
 */
#define PALERT_M4_DIO_STATUS_GET(_PAM4H, _DIO_NUMBER) \
		((_DIO_NUMBER) < 8 ? \
		((_PAM4H)->dio_status[0] & (0x01 << (_DIO_NUMBER))) : \
		((_PAM4H)->dio_status[1] & (0x01 << (_DIO_NUMBER) - 8)))

/**
 * @brief
 *
 */
#define PALERT_PKT_IS_MODE4(_PAPKT) \
		(!(((uint8_t *)(_PAPKT))[0] ^ 0x04))

/* Export functions's prototypes */
char *pac_m4_trigmode_get( const PALERT_M4_HEADER * );
char *pac_m4_ip_get( const PALERT_M4_HEADER *, const int, char * );
