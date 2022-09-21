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
#include <stdint.h>
/* */
#include <libmseed.h>
#include <trace_buf.h>
/* */
#include <palert.h>

/* */
#define PA2EW_INFO_FROM_SQL       4
#define PA2EW_DEF_CHAN_PER_STA    3
#define PA2EW_TCP_SYNC_ERR_LIMIT  15
#define PA2EW_NTP_SYNC_ERR_LIMIT  30
/* */
#define PA2EW_PALERT_PORT            "502"
#define PA2EW_PALERT_EXT_PORT        "24000"
#define PA2EW_MAX_PALERTS_PER_THREAD  512
#define PA2EW_IDLE_THRESHOLD          120
#define PA2EW_RECONNECT_INTERVAL      15000
/* */
#define PA2EW_RECV_SERVER_OFF  0
#define PA2EW_RECV_SERVER_ON   1
/* */
#define PA2EW_EXT_FUNC_OFF     0
#define PA2EW_EXT_FUNC_ON      1
/* */
#define PA2EW_RECV_BUFFER_LENGTH  65536
#define PA2EW_RECV_NORMAL         0
#define PA2EW_RECV_NEED_UPDATE   -1
#define PA2EW_RECV_CONNECT_ERROR -2
#define PA2EW_RECV_FATAL_ERROR   -3
/* */
#define PA2EW_MSG_SERVER_NORMAL  0
#define PA2EW_MSG_SERVER_EXT     1
#define PA2EW_MSG_CLIENT_STREAM  2
#define PA2EW_MSG_LIST_BLOCK     3   /* Unused */
#define PA2EW_MSG_MSG_QUEUE      4   /* Unused */
/* */
typedef union {
	void    *staptr;
	uint16_t serial;
} LABEL;

/* */
typedef struct {
/* */
	LABEL   label;
	uint8_t recv_buffer[PA2EW_RECV_BUFFER_LENGTH];
} LABELED_RECV_BUFFER;

/* Station info related struct */
typedef struct {
	uint8_t  update;
	uint8_t  ntp_errors;
	int8_t   ext_flag;
	char     sta[TRACE2_STA_LEN];
	char     net[TRACE2_NET_LEN];
	char     loc[TRACE2_LOC_LEN];
	uint16_t serial;
	uint16_t nchannel;
/* */
	void *chaptr;
/* */
	void *buffer;
} _STAINFO;

/* */
typedef struct {
	uint8_t seq;
	char    chan[TRACE2_CHAN_LEN];
	double  last_endtime;
} _CHAINFO;

/* Streamline mini-SEED data record structures */
typedef struct {
	struct fsdh_s fsdh;
	uint16_t blkt_type;
	uint16_t next_blkt;
	struct blkt_1000_s blkt1000;
	uint8_t smsrlength[2];
	uint8_t reserved[6];
} SMSRECORD;
