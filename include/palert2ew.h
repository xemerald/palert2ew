/*
 * palert2ew.h
 *
 * Header file of main program, that define those general constants & struct.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * August, 2020
 *
 */
#pragma once

/* */
#define PA2EW_INFO_FROM_SQL       4
#define PA2EW_DEF_CHAN_PER_STA    3
#define PA2EW_NTP_SYNC_ERR_LIMIT  30
/* */
#define PA2EW_PALERT_PORT            "502"
#define PA2EW_MAX_PALERTS_PER_THREAD  512
/* */
#define PA2EW_RECV_BUFFER_LENGTH  12000
#define PA2EW_PREPACKET_LENGTH    (PA2EW_RECV_BUFFER_LENGTH + 4)
/* */
#define PA2EW_RECV_NORMAL         0
#define PA2EW_RECV_NEED_UPDATE   -1
#define PA2EW_RECV_CONNECT_ERROR -2
#define PA2EW_RECV_FATAL_ERROR   -3

/* */
#include <trace_buf.h>
/* */
#include <palert.h>

/* */
typedef struct {
	uint16_t serial;
	uint16_t len;
	uint8_t  data[PA2EW_RECV_BUFFER_LENGTH];
} PREPACKET;

/* Internal stack related struct */
typedef struct {
	void    *sptr;
	uint8_t  data[PALERTMODE1_PACKET_LENGTH];
} PACKET;

/* */
typedef struct {
	uint8_t  header_ready;
	uint8_t  ntp_errors;
	uint16_t packet_rear;
} PACKETPARAM;

/* */
typedef struct {
	uint16_t serial;
	int32_t  timestamp;
	uint16_t samprate;
	uint8_t  packet_mode;
} RT_HEARTBEAT;

/* */
typedef struct {
	int16_t sensor_status;
	int16_t cpu_temp;
	int16_t ext_volt;
	int16_t int_volt;
	int16_t rtc_battery;
	uint8_t ntp_status;
	uint8_t gnss_status;
	uint8_t gps_lock;
	int16_t satellite_num;
	double  latitude;
	double  longitude;
} SOH_PACKET;

/* Station info related struct */
typedef struct {
	uint16_t serial;
	uint16_t nchannel;
	char     sta[TRACE2_STA_LEN];
	char     net[TRACE2_NET_LEN];
	char     loc[TRACE2_LOC_LEN];
	void    *chaptr;
/* */
	PACKET      packet;
	PACKETPARAM param;
/* */
	void    *extptr;
} _STAINFO;

/* */
typedef struct {
	uint8_t seq;
	char    chan[TRACE2_CHAN_LEN];
	double  last_endtime;
} _CHAINFO;
