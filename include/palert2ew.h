/*
 *
 */
#pragma once

/* */
#define PA2EW_INFO_FROM_SQL     4
#define PA2EW_DEF_CHAN_PER_STA  3
/* */
#define DATA_BUFFER_LENGTH  12000
#define PREPACKET_LENGTH    DATA_BUFFER_LENGTH + 4

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
	uint8_t  data[PALERTPACKET_LENGTH];
} PACKET;

/* */
typedef struct {
	uint16_t NtpErrCnt;
	uint16_t PacketRear;
	uint16_t PacketReq;
} PACKETPARAM;

/* Station info related struct */
typedef struct {
	uint16_t serial;
	uint16_t nchannel;
	char     sta[TRACE2_STA_LEN];
	char     net[TRACE2_NET_LEN];
	char     loc[TRACE2_LOC_LEN];
	void    *chaptr;

	PACKET      packet;
	PACKETPARAM param;
} _STAINFO;

/* */
typedef struct {
	uint8_t seq;
	char    chan[TRACE2_CHAN_LEN];
} _CHAINFO
