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
#include <trace_buf.h>
/* */
#include <palert.h>

/* */
#define PA2EW_INFO_FROM_SQL       4
#define PA2EW_DEF_CHAN_PER_STA    3
#define PA2EW_TCP_SYNC_ERR_LIMIT  10
#define PA2EW_NTP_SYNC_ERR_LIMIT  30
/* */
#define PA2EW_PALERT_PORT            "502"
#define PA2EW_PALERT_EXT_PORT        "24000"
#define PA2EW_MAX_PALERTS_PER_THREAD  512
#define PA2EW_IDLE_THRESHOLD          120
/* */
#define PA2EW_RECV_BUFFER_LENGTH  16384
#define PA2EW_RECV_NORMAL         0
#define PA2EW_RECV_NEED_UPDATE   -1
#define PA2EW_RECV_CONNECT_ERROR -2
#define PA2EW_RECV_FATAL_ERROR   -3

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
	void *raw_conn;
	void *ext_conn;
} _STAINFO;

/* */
typedef struct {
	uint8_t seq;
	char    chan[TRACE2_CHAN_LEN];
	double  last_endtime;
} _CHAINFO;
