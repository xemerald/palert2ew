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
/* Packet infomation */
	uint8_t  packet_type[2];
	uint8_t  event_flag[2];
/* System time infomation */
	uint8_t  sys_year[2];
	uint8_t  sys_month[2];
	uint8_t  sys_day[2];
	uint8_t  sys_hour[2];
	uint8_t  sys_minute[2];
	uint8_t  sys_tenmsec;
	uint8_t  sys_second;
/* Event time infomation */
	uint8_t  ev_year[2];
	uint8_t  ev_month[2];
	uint8_t  ev_day[2];
	uint8_t  ev_hour[2];
	uint8_t  ev_minute[2];
	uint8_t  ev_tenmsec;
	uint8_t  ev_second;
/* Hardware infomation */
	uint8_t  serial_no[2];
/* Warning & watch setting */
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
/* Some value for EEW */
	uint8_t  intensity_now[2];
	uint8_t  intensity_max[2];
	uint8_t  pga_1s[2];
	uint8_t  pga_1s_axis[2];
	uint8_t  tauc[2];
	uint8_t  trig_mode[2];
	uint8_t  op_mode[2];
	uint8_t  dura_watch_warning[2];
/* Firmware version */
	uint8_t  firmware[2];
/* Network infomation */
	uint16_t palert_ip[4];
	uint8_t  tcp0_server[4];
	uint8_t  tcp1_server[4];
	uint16_t ntp_server[4];
	uint8_t  socket_remain[2];
	uint8_t  connection_flag[2];
/* EEW status */
	uint8_t  dio_status[2];
	uint8_t  eew_register[2];
/* Sensor summary */
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
/* reserved byte */
	uint8_t  reserved_1[18];
/* synchronized character */
	uint8_t  sync_char[8];
/* Packet length */
	uint8_t  packet_len[2];
/* EEW DO intensity */
	uint8_t  eews_do0_intensity[2];
	uint8_t  eews_do1_intensity[2];
/* FTE-D04 information */
	uint8_t  fte_d04_server[4];
/* Operation mode X */
	uint8_t  op_mode_x[2];
/* White list information */
	uint8_t  whitelist_1[4];
	uint8_t  whitelist_2[4];
	uint8_t  whitelist_3[4];
/* Maximum vector velocity */
	uint8_t  vel_vector_max[2];
/* reserved byte */
	uint8_t  reserved_2[24];
/* Sampling rate */
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
#define PALERT_M1H_OP_GBT_INTENSITY         0x0001
#define PALERT_M1H_OP_GAS_MODE              0x0002
#define PALERT_M1H_OP_INTENSITY_VECTOR      0x0004
#define PALERT_M1H_OP_CONNECT_TCP0          0x0008
#define PALERT_M1H_OP_CONNECT_NTP           0x0010
#define PALERT_M1H_OP_MODE_DHCP             0x0020
#define PALERT_M1H_OP_CONNECT_TCP1          0x0040
#define PALERT_M1H_OP_CONNECT_SANLIEN       0x0080
#define PALERT_M1H_OP_MODBUS_TCP_CLIENT     0x8000
/* 40208 Operation mode extension flags */
#define PALERT_M1H_OPX_WHITELIST_BIT        0x0001
#define PALERT_M1H_OPX_HORIZON_INSTALL_BIT  0x0002
#define PALERT_M1H_OPX_CWB2020_BIT          0x0004
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
		((_PAM1H)->sync_char[0] == 0x30 && (_PAM1H)->sync_char[1] == 0x33 && \
		(_PAM1H)->sync_char[2] == 0x30 && (_PAM1H)->sync_char[3] == 0x35 && \
		(_PAM1H)->sync_char[4] == 0x31 && (_PAM1H)->sync_char[5] == 0x35 && \
		(_PAM1H)->sync_char[6] == 0x30 && (_PAM1H)->sync_char[7] == 0x31)

/**
 * @brief Parse the palert packet length
 *
 */
#define PALERT_M1_PACKETLEN_GET(_PAM1H) \
		PALERT_M1_WORD_GET((_PAM1H)->packet_len)

/**
 * @brief Parse the palert serial number
 *
 */
#define PALERT_M1_SERIAL_GET(_PAM1H) \
		PALERT_M1_WORD_GET((_PAM1H)->serial_no)

/**
 * @brief Parse the palert firmware version
 *
 */
#define PALERT_M1_FIRMWARE_GET(_PAM1H) \
		PALERT_M1_WORD_GET((_PAM1H)->firmware)

/**
 * @brief Parse the palert packet type
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
 * @brief Parse the palert sampling rate
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
		((((uint8_t *)(_PAPKT))[0] ^ 0x04) && PALERT_M1_PACKETLEN_GET( (PALERT_M1_HEADER *)_PAPKT ) == PALERT_M1_PACKET_LENGTH)

/**
 * @brief
 *
 */
#define PALERT_PKT_IS_MODE2(_PAPKT) \
		((((uint8_t *)(_PAPKT))[0] ^ 0x04) && PALERT_M1_PACKETLEN_GET( (PALERT_M1_HEADER *)_PAPKT ) == PALERT_M2_HEADER_LENGTH)

/* Export functions's prototypes */
double pac_m1_systime_get( const PALERT_M1_HEADER *, long );
double pac_m1_evtime_get( const PALERT_M1_HEADER *, long );
char  *pac_m1_chan_code_get( const int );
double pac_m1_chan_unit_get( const int );
char  *pac_m1_trigmode_get( const PALERT_M1_HEADER * );
char  *pac_m1_ip_get( const PALERT_M1_HEADER *, const int, char * );
void   pac_m1_data_extract( const PALERT_M1_PACKET *, int32_t *[PALERT_M1_CHAN_COUNT] );
