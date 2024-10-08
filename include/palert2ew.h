/**
 * @file palert2ew.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file of main program, that define those general constants & struct.
 * @date 2020-08-01
 *
 * @copyright Copyright (c) 2020
 *
 */

#pragma once

/**
 * @name Standard C header include
 *
 */
#include <stdint.h>

/**
 * @name Earthworm environment header include
 *
 */
#include <libmseed.h>
#include <trace_buf.h>

/**
 * @brief Program release information
 *
 */
#define PALERT2EW_VERSION "2.1.0"
#define PALERT2EW_RELEASE "2024.06.26"
#define PALERT2EW_AUTHOR  "Benjamin Ming Yang"
/* */
#define PA2EW_INFO_FROM_SQL       4
#define PA2EW_DEF_CHAN_PER_STA    3
#define PA2EW_MAX_CHAN_PER_STA    8
#define PA2EW_TCP_SYNC_ERR_LIMIT  15
#define PA2EW_NTP_SYNC_ERR_LIMIT  30
/* */
#define PA2EW_RECV_SERVER_CRC8_INIT  0x00
#define PA2EW_RECV_SERVER_CRC8_POLY  0x07
/* */
#define PA2EW_PALERT_PORT            "502"
#define PA2EW_MAX_PALERTS_PER_THREAD  512
#define PA2EW_IDLE_THRESHOLD          120
#define PA2EW_RECONNECT_INTERVAL      15000
/* */
#define PA2EW_RECV_SERVER_OFF  0
#define PA2EW_RECV_SERVER_ON   1
/* */
#define PA2EW_RECV_BUFFER_LENGTH  65536
#define PA2EW_RECV_NORMAL         0
#define PA2EW_RECV_NEED_UPDATE   -1
#define PA2EW_RECV_CONNECT_ERROR -2
#define PA2EW_RECV_FATAL_ERROR   -3
/* */
#define PA2EW_MSG_SERVER_NORMAL  0
#define PA2EW_MSG_CLIENT_STREAM  1
#define PA2EW_MSG_LIST_BLOCK     2   /* Unused */
#define PA2EW_MSG_MSG_QUEUE      3   /* Unused */
/* */
#define PA2EW_UNKNOWN_ENDIAN  0
#define PA2EW_LITTLE_ENDIAN   1
#define PA2EW_BIG_ENDIAN      2
#define PA2EW_PDP_ENDIAN      3

/**
 * @brief
 *
 */
typedef struct {
	void    *staptr;
	uint16_t packmode;
} LABEL;

/**
 * @brief
 *
 */
typedef struct {
	LABEL   label;
	uint8_t recv_buffer[PA2EW_RECV_BUFFER_LENGTH];
} LABELED_RECV_BUFFER;

/**
 * @brief Station info related struct
 *
 */
typedef struct {
	uint8_t  update;
	uint8_t  ntp_errors;
	char     sta[TRACE2_STA_LEN];
	char     net[TRACE2_NET_LEN];
	char     loc[TRACE2_LOC_LEN];
	uint16_t serial;
	uint16_t nchannel;
	int64_t  timeshift;
/* */
	void *chaptr;
/* */
	void *buffer;
} _STAINFO;

/**
 * @brief
 *
 */
typedef struct {
	uint8_t seq;
	char    chan[TRACE2_CHAN_LEN];
	double  last_endtime;
} _CHAINFO;

/**
 * @brief Streamline mini-SEED data record structures
 *
 */
typedef struct {
	struct fsdh_s fsdh;
	uint16_t blkt_type;
	uint16_t next_blkt;
	struct blkt_1000_s blkt1000;
	uint8_t smsrlength[2];
	uint8_t reserved[6];
} SMSRECORD;
