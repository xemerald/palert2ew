/*
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
#define DATA_BUFFER_LENGTH  12000
#define PREPACKET_LENGTH    (DATA_BUFFER_LENGTH + 4)

/* */
#include <trace_buf.h>
/* */
#include <palert.h>

/* */
typedef struct {
	uint16_t serial;
	uint16_t len;
	uint8_t  data[DATA_BUFFER_LENGTH];
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
	uint16_t packet_req;
} PACKETPARAM;

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
} _STAINFO;

/* */
typedef struct {
	uint8_t seq;
	char    chan[TRACE2_CHAN_LEN];
} _CHAINFO;
