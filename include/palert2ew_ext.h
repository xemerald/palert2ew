/*
 * palert2ew_ext.h
 *
 * Header file for extension functions of Palerts.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * February, 2022
 *
 */
#pragma once
/* */
#include <stdint.h>
/* */
#include <palert.h>
#include <palert2ew.h>
/* */
#define PA2EW_EXT_HEADER_SIZE           8
#define PA2EW_EXT_REQUEST_RETRY_LIMIT   3
#define PA2EW_EXT_CONNECT_RETRY_LIMIT   3
#define PA2EW_EXT_REQUEST_SOH_INTERVAL  60
#define PA2EW_EXT_MAX_PACKET_SIZE       4096
#define PA2EW_EXT_SOH_INTVALUE_UNIT     0.01
/* */
#define PA2EW_EXT_TYPE_HEARTBEAT   0
#define PA2EW_EXT_TYPE_RT_PACKET   1
#define PA2EW_EXT_TYPE_SOH_PACKET  2
/* */
#define PA2EW_EXT_RT_COMMAND_FORMAT   "%d:%d:%ld"
#define PA2EW_EXT_SOH_COMMAND_FORMAT  "%d:SOH:%ld"

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
		EXT_HEADER     header;
		EXT_SOH_PACKET soh_packet;
	} soh;
/* */
	uint8_t buffer[PA2EW_EXT_MAX_PACKET_SIZE];
} PalertExtPacket;

/* */
typedef struct {
	_STAINFO   *staptr;
	_CHAINFO   *chaptr;
	double      lastend;
	double      starttime;
	double      endtime;
} __EXT_COMMAND_ARG;

/**/
thr_ret pa2ew_ext_rt_req_thread( void * );
thr_ret pa2ew_ext_soh_req_thread( void * );
int     pa2ew_ext_rt_packet_process( void *, PalertExtPacket *, _STAINFO *, const int );
int     pa2ew_ext_soh_packet_process( void *, PalertExtPacket *, _STAINFO * );
/* */
__EXT_COMMAND_ARG *pa2ew_ext_req_queue_insert( void **, _STAINFO *, _CHAINFO *, double, double, double );
