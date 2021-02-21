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
#include <trace_buf.h>
/* */
#include <palert.h>

/* */
#define PA2EW_INFO_FROM_SQL       4
#define PA2EW_DEF_CHAN_PER_STA    3
#define PA2EW_NTP_SYNC_ERR_LIMIT  30
/* */
#define PA2EW_PALERT_PORT            "502"
#define PA2EW_PALERT_EXT_PORT        "24000"
#define PA2EW_MAX_PALERTS_PER_THREAD  512
/* */
#define PA2EW_RECV_BUFFER_LENGTH  16384
#define PA2EW_RECV_NORMAL         0
#define PA2EW_RECV_NEED_UPDATE   -1
#define PA2EW_RECV_CONNECT_ERROR -2
#define PA2EW_RECV_FATAL_ERROR   -3
/* */
#define PA2EW_EXT_HEADER_SIZE      8
#define PA2EW_EXT_MAX_PACKET_SIZE  4096
/* */
#define PA2EW_EXT_TYPE_HEARTBEAT   0
#define PA2EW_EXT_TYPE_RT_PACKET   1
#define PA2EW_EXT_TYPE_SOH_PACKET  2
/* */
#define PA2EW_EXT_RT_COMMAND_FORMAT "%d:%d:%ld"

/* */
typedef struct {
	uint16_t serial;
	uint16_t ext_type;
	uint32_t length;
} EXT_HEADER;

/* */
typedef struct {
	int32_t  timestamp;
	uint16_t samprate;
	uint16_t nsamp;
	uint8_t  data_bytes;
	uint8_t  chan_seq;
	uint8_t  padding[6];
	uint8_t  data[0];
} EXT_RT_PACKET;

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
	uint8_t padding;
	int16_t satellite_num;
	double  latitude;
	double  longitude;
} EXT_SOH_PACKET;

/* */
typedef union {
/* */
	EXT_HEADER header;
/* */
	struct {
		EXT_HEADER    header;
		EXT_RT_PACKET rt_packet;
	} rt;
/* */
	struct {
		EXT_HEADER    header;
		EXT_RT_PACKET soh_packet;
	} soh;
/* */
	uint8_t buffer[PA2EW_EXT_MAX_PACKET_SIZE];
} PalertExtPacket;

/* Internal stack related struct */
typedef struct {
/* */
	void *sptr;
/* */
	union {
		PalertPacket    palert_pck;
		PalertExtPacket palert_ext_pck;
	} data;
} LABELED_DATA;

/* */
typedef struct {
/* */
	void   *sptr;
	uint8_t recv_buffer[PA2EW_RECV_BUFFER_LENGTH];
} LABELED_RECV_BUFFER;

/* Station info related struct */
typedef struct {
	uint8_t  ntp_errors;;
	char     sta[TRACE2_STA_LEN];
	char     net[TRACE2_NET_LEN];
	char     loc[TRACE2_LOC_LEN];
	uint16_t serial;
	uint16_t nchannel;
/* */
	void *chaptr;
	void *msg_buffer;
	void *req_queue;
	void *raw_conn;
	void *ext_conn;
} _STAINFO;

/* */
typedef struct {
	uint8_t seq;
	char    chan[TRACE2_CHAN_LEN];
	double  last_endtime;
} _CHAINFO;
