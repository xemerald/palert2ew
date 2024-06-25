/**
 * @file mode1.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for Palert mode 1 & 2 data packet.
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
#define PALERT_M1_HEADER_LENGTH    200
#define PALERT_M2_HEADER_LENGTH    PALERT_M1_HEADER_LENGTH
#define PALERT_M1_PACKET_LENGTH    1200
#define PALERT_M2_PACKET_LENGTH    PALERT_M1_HEADER_LENGTH

/*
 * Definition of Palert mode 1 & 2 header, total size is 200 bytes
 */
typedef struct {
/* Packet infomation, start at #0 byte */
	uint8_t  packet_type[2];
	uint8_t  event_flag[2];
/* System time infomation, start at #4 byte */
	uint8_t  sys_year[2];
	uint8_t  sys_month[2];
	uint8_t  sys_day[2];
	uint8_t  sys_hour[2];
	uint8_t  sys_minute[2];
	uint8_t  sys_tenmsec;
	uint8_t  sys_second;
/* Event time infomation, start at #16 byte */
	uint8_t  ev_year[2];
	uint8_t  ev_month[2];
	uint8_t  ev_day[2];
	uint8_t  ev_hour[2];
	uint8_t  ev_minute[2];
	uint8_t  ev_tenmsec;
	uint8_t  ev_second;
/* Hardware infomation, start at #28 byte */
	uint8_t  serial_no[2];
/* Warning & watch setting, start at #30 byte */
	uint8_t  dis_watch_threshold[2];
	uint8_t  pgv_1s[2];
	uint8_t  pgd_1s[2];
	uint8_t  pga_10s[2];
	uint8_t  pga_trig_axis[2];
	uint8_t  pd_warning_threshold[2];
	uint8_t  pga_warning_threshold[2];
	uint8_t  dis_warning_threshold[2];
	uint8_t  pd_flag[2];
	uint8_t  pd_watch_threshold[2];
	uint8_t  pga_watch_threshold[2];
/* Some value for EEW, start at #52 byte */
	uint8_t  intensity_now[2];
	uint8_t  intensity_max[2];
	uint8_t  pga_1s[2];
	uint8_t  pga_1s_axis[2];
	uint8_t  tauc[2];
	uint8_t  trig_mode[2];
	uint8_t  op_mode[2];
	uint8_t  dura_watch_warning[2];
/* Firmware version, start at #68 byte */
	uint8_t  firmware[2];
/* Network infomation, start at #70 byte */
	uint16_t palert_ip[4];
	uint8_t  tcp0_server[4];
	uint8_t  tcp1_server[4];
	uint16_t ntp_server[4];
	uint8_t  socket_remain[2];
	uint8_t  connection_flag[2];
/* EEW status, start at #98 byte */
	uint8_t  dio_status[2];
	uint8_t  eew_register[2];
/* Sensor summary, start at #102 byte */
	uint8_t  pd_vertical[2];
	uint8_t  pv_vertical[2];
	uint8_t  pa_vertical[2];
	uint8_t  vector_max[2];
	uint8_t  acc_a_max[2];
	uint8_t  acc_b_max[2];
	uint8_t  acc_c_max[2];
	uint8_t  accvec_a_max[2];
	uint8_t  accvec_b_max[2];
	uint8_t  accvec_c_max[2];
/* reserved_0 byte, start at #122 byte */
	uint8_t  reserved_0[18];
/* synchronized character, start at #140 byte */
	uint8_t  sync_char[8];
/* Packet length, start at #148 byte */
	uint8_t  packet_len[2];
/* EEW DO intensity, start at #150 byte */
	uint8_t  eews_do0_intensity[2];
	uint8_t  eews_do1_intensity[2];
/* FTE-D04 information, start at #154 byte */
	uint8_t  fte_d04_server[4];
/* Operation mode X, start at #158 byte */
	uint8_t  op_mode_x[2];
/* White list information, start at #160 byte */
	uint8_t  whitelist_1[4];
	uint8_t  whitelist_2[4];
	uint8_t  whitelist_3[4];
/* Maximum vector velocity, start at #172 byte */
	uint8_t  vel_vector_max[2];
/* reserved_1 byte, start at #174 byte */
	uint8_t  reserved_1[12];
/* CRC-16 check sum, start at #186 byte */
	uint8_t  crc16_byte[2];
/* reserved_1 byte, start at #188 byte */
	uint8_t  reserved_2[10];
/* Sampling rate, start at #198 byte */
	uint8_t  samprate[2];
} PALERT_M1_HEADER;

/* Alias of the structure above */
typedef PALERT_M1_HEADER PAM1H;

/* */
#define PALERT_M1_VEC_UNIT                  0.1f          /* count to gal    */
#define PALERT_M1_ACC_UNIT                  0.059814453f  /* count to gal    */
#define PALERT_M1_VEL_UNIT                  0.01f         /* count to cm/sec */
#define PALERT_M1_DIS_UNIT                  0.001f        /* count to cm     */
/* 40118 Operation mode flags */
#define PALERT_M1_OP_GBT_INT_BIT            0x0001
#define PALERT_M1_OP_GAS_MODE_BIT           0x0002
#define PALERT_M1_OP_INTENSITY_VEC_BIT      0x0004
#define PALERT_M1_OP_CONNECT_TCP0_BIT       0x0008
#define PALERT_M1_OP_CONNECT_NTP_BIT        0x0010
#define PALERT_M1_OP_DHCP_ENABLE_BIT        0x0020
#define PALERT_M1_OP_CONNECT_TCP1_BIT       0x0040
#define PALERT_M1_OP_CONNECT_SANLIEN_BIT    0x0080
#define PALERT_M1_OP_CONNECT_FTED04_BIT     0x0100
#define PALERT_M1_OP_MMI_INT_BIT            0x0200
#define PALERT_M1_OP_KMA_INT_BIT            0x0400
#define PALERT_M1_OP_MODBUS_TCP_CLIENT_BIT  0x8000
/* 40208 Operation mode extension flags */
#define PALERT_M1_OPX_WHITELIST_BIT         0x0001
#define PALERT_M1_OPX_HORIZON_INSTALL_BIT   0x0002
#define PALERT_M1_OPX_CWB2020_BIT           0x0004
#define PALERT_M1_OPX_HIGHRATE_NTP_BIT      0x0008
#define PALERT_M1_OPX_MODE2PKT_BIT          0x0010
/* */
#define PALERT_M1_SAMPLE_NUMBER             100
/* */
#define PALERT_M1_PACKETTYPE_NORMAL         1
#define PALERT_M1_PACKETTYPE_PWAVE          119
#define PALERT_M1_PACKETTYPE_PD3            300
#define PALERT_M1_PACKETTYPE_PDWATCH        1191
#define PALERT_M1_PACKETTYPE_PDWARNING      1192

/*
 * Palert mode 1 default channel information
 */
#define PALERT_M1_CHAN_TABLE \
		X(PALERT_M1_CHAN_0,     "HLZ",  PALERT_M1_ACC_UNIT) \
		X(PALERT_M1_CHAN_1,     "HLN",  PALERT_M1_ACC_UNIT) \
		X(PALERT_M1_CHAN_2,     "HLE",  PALERT_M1_ACC_UNIT) \
		X(PALERT_M1_CHAN_3,     "PD",   PALERT_M1_DIS_UNIT) \
		X(PALERT_M1_CHAN_4,     "DIS",  PALERT_M1_DIS_UNIT) \
		X(PALERT_M1_CHAN_COUNT, "NULL", 0                  )

#define X(a, b, c) a,
typedef enum {
	PALERT_M1_CHAN_TABLE
} PALERT_M1_CHANNEL;
#undef X

/*
 * Definition of Palert mode 1 data block structure, total size is 10 bytes
 */
typedef union {
	struct {
		uint8_t acc[3][2];
		uint8_t pd[2];
		uint8_t dis[2];
	};
	uint8_t cmp[PALERT_M1_CHAN_COUNT][2];
} PALERT_M1_DATA;

/* Alias of the structure above */
typedef PALERT_M1_DATA PAM1D;

/*
 * Definition of Palert generic mode 1 packet structure, total size is 1200 bytes
 */
typedef struct {
	PALERT_M1_HEADER header;
	PALERT_M1_DATA   data[PALERT_M1_SAMPLE_NUMBER];
} PALERT_M1_PACKET;

/* Alias of the structure above */
typedef PALERT_M1_PACKET PAM1P;

/**
 * @brief
 *
 */
#define PALERT_M1_SYNC_CHAR_0  0x30  /* Equal to ASCII '0' */
#define PALERT_M1_SYNC_CHAR_1  0x33  /* Equal to ASCII '3' */
#define PALERT_M1_SYNC_CHAR_2  0x30  /* Equal to ASCII '0' */
#define PALERT_M1_SYNC_CHAR_3  0x35  /* Equal to ASCII '5' */
#define PALERT_M1_SYNC_CHAR_4  0x31  /* Equal to ASCII '1' */
#define PALERT_M1_SYNC_CHAR_5  0x35  /* Equal to ASCII '5' */
#define PALERT_M1_SYNC_CHAR_6  0x30  /* Equal to ASCII '0' */
#define PALERT_M1_SYNC_CHAR_7  0x31  /* Equal to ASCII '1' */

/**
 * @brief All the data withing the mode 1 packet should be Little-Endian
 *
 */
#define PALERT_M1_WORD_GET(_PAM1H_WORD) \
		(((uint16_t)((_PAM1H_WORD)[1]) << 8) | (uint16_t)((_PAM1H_WORD)[0]))

/**
 * @brief
 *
 */
#define PALERT_M1_NTP_CHECK(_PAM1H) \
		((_PAM1H)->connection_flag[0] & 0x01)

/**
 * @brief
 *
 */
#define PALERT_M1_SYNC_CHECK(_PAM1H) \
		(((_PAM1H)->sync_char[0] == PALERT_M1_SYNC_CHAR_0) && ((_PAM1H)->sync_char[1] == PALERT_M1_SYNC_CHAR_1) && \
		((_PAM1H)->sync_char[2] == PALERT_M1_SYNC_CHAR_2) && ((_PAM1H)->sync_char[3] == PALERT_M1_SYNC_CHAR_3) && \
		((_PAM1H)->sync_char[4] == PALERT_M1_SYNC_CHAR_4) && ((_PAM1H)->sync_char[5] == PALERT_M1_SYNC_CHAR_5) && \
		((_PAM1H)->sync_char[6] == PALERT_M1_SYNC_CHAR_6) && ((_PAM1H)->sync_char[7] == PALERT_M1_SYNC_CHAR_7))

/**
 * @brief Parse the Palert mode 1 packet length
 *
 */
#define PALERT_M1_PACKETLEN_GET(_PAM1H) \
		PALERT_M1_WORD_GET((_PAM1H)->packet_len)

/**
 * @brief Parse the Palert mode 1 serial number
 *
 */
#define PALERT_M1_SERIAL_GET(_PAM1H) \
		PALERT_M1_WORD_GET((_PAM1H)->serial_no)

/**
 * @brief Parse the Palert mode 1 firmware version
 *
 */
#define PALERT_M1_FIRMWARE_GET(_PAM1H) \
		PALERT_M1_WORD_GET((_PAM1H)->firmware)

/**
 * @brief Parse the Palert mode 1 packet type
 *
 */
#define PALERT_M1_PACKETTYPE_GET(_PAM1H) \
		PALERT_M1_WORD_GET((_PAM1H)->packet_type)

/**
 * @brief
 *
 */
#define PALERT_M1_DIO_STATUS_GET(_PAM1H, _DIO_NUMBER) \
		((_DIO_NUMBER) < 8 ? \
		((_PAM1H)->dio_status[0] & (0x01 << (_DIO_NUMBER))) : \
		((_PAM1H)->dio_status[1] & (0x01 << (_DIO_NUMBER) - 8)))

/**
 * @brief Parse the Palert mode 1 sampling rate
 *
 */
#define PALERT_M1_SAMPRATE_GET(_PAM1H) \
		((PALERT_M1_WORD_GET((_PAM1H)->samprate) && PALERT_M1_WORD_GET((_PAM1H)->samprate) <= PALERT_MAX_SAMPRATE) ? \
		PALERT_M1_WORD_GET((_PAM1H)->samprate) : PALERT_DEFAULT_SAMPRATE)

/**
 * @brief
 *
 */
#define PALERT_M1_IS_CWB2020(_PAM1H) \
		((_PAM1H)->op_mode_x[0] & PALERT_M1H_OPX_CWB2020_BIT)

/**
 * @brief
 *
 */
#define PALERT_PKT_IS_MODE1(_PAPKT) \
		(((((uint8_t *)(_PAPKT))[140] == PALERT_M1_SYNC_CHAR_0) && (((uint8_t *)(_PAPKT))[141] == PALERT_M1_SYNC_CHAR_1) && \
		(((uint8_t *)(_PAPKT))[142] == PALERT_M1_SYNC_CHAR_2) && (((uint8_t *)(_PAPKT))[143] == PALERT_M1_SYNC_CHAR_3) && \
		(((uint8_t *)(_PAPKT))[144] == PALERT_M1_SYNC_CHAR_4) && (((uint8_t *)(_PAPKT))[145] == PALERT_M1_SYNC_CHAR_5) && \
		(((uint8_t *)(_PAPKT))[146] == PALERT_M1_SYNC_CHAR_6) && (((uint8_t *)(_PAPKT))[147] == PALERT_M1_SYNC_CHAR_7)) && \
		(PALERT_M1_PACKETLEN_GET( (PALERT_M1_HEADER *)_PAPKT ) == PALERT_M1_PACKET_LENGTH))

/**
 * @brief
 *
 */
#define PALERT_PKT_IS_MODE2(_PAPKT) \
		(((((uint8_t *)(_PAPKT))[140] == PALERT_M1_SYNC_CHAR_0) && (((uint8_t *)(_PAPKT))[141] == PALERT_M1_SYNC_CHAR_1) && \
		(((uint8_t *)(_PAPKT))[142] == PALERT_M1_SYNC_CHAR_2) && (((uint8_t *)(_PAPKT))[143] == PALERT_M1_SYNC_CHAR_3) && \
		(((uint8_t *)(_PAPKT))[144] == PALERT_M1_SYNC_CHAR_4) && (((uint8_t *)(_PAPKT))[145] == PALERT_M1_SYNC_CHAR_5) && \
		(((uint8_t *)(_PAPKT))[146] == PALERT_M1_SYNC_CHAR_6) && (((uint8_t *)(_PAPKT))[147] == PALERT_M1_SYNC_CHAR_7)) && \
		(PALERT_M1_PACKETLEN_GET( (PALERT_M1_HEADER *)_PAPKT ) == PALERT_M2_PACKET_LENGTH))

/* Export functions's prototypes */
double pac_m1_systime_get( const PALERT_M1_HEADER *, long );
double pac_m1_evtime_get( const PALERT_M1_HEADER *, long );
char  *pac_m1_chan_code_get( const int );
double pac_m1_chan_unit_get( const int );
char  *pac_m1_trigmode_get( const PALERT_M1_HEADER * );
char  *pac_m1_ip_get( const PALERT_M1_HEADER *, const int, char * );
void   pac_m1_data_extract( const PALERT_M1_PACKET *, int32_t *[PALERT_M1_CHAN_COUNT] );
int    pac_m1_crc_check( const PALERT_M1_PACKET * );
