/*
 *
 */

#ifdef _OS2
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <lockfile.h>


/* */
typedef struct {
	_STAINFO   *staptr;
	_CHAINFO   *chaptr;
	double      lastend;
	double      starttime;
	double      endtime;
} __EXT_COMMAND_ARG;

/* Functions prototype in this source file
 *******************************/
static thr_ret request_rt_thread( void * );
static thr_ret request_soh_thread( void * );

static void           process_packet_rt( PalertExtPacket *, _STAINFO * );
static void           process_packet_soh( PalertExtPacket *, _STAINFO * );
static int32_t       *copydata_tracebuf_rt( const EXT_RT_PACKET *, int32_t * );
static void           request_soh_stations( const void *, const VISIT, const int );
static TRACE2_HEADER *enrich_trh2_rt( TRACE2_HEADER *, const _STAINFO *, const EXT_RT_PACKET * );
static __EXT_COMMAND_ARG *insert_request_queue(
	__EXT_COMMAND_ARG **, _STAINFO *, _CHAINFO *, double, double, double
);
static __EXT_COMMAND_ARG *create_request_queue( const int );
static __EXT_COMMAND_ARG *enrich_ext_command_arg( __EXT_COMMAND_ARG *, _STAINFO *, _CHAINFO *, double, double, double );


#if defined( _V710 )
static ew_thread_t      ReqSOHThreadID      = 0;          /* Thread id for requesting SOH */
#else
static unsigned         ReqSOHThreadID      = 0;          /* Thread id for requesting SOH */
#endif


/*
 *
 */
thr_ret request_rt_thread( void *arg )
{
	__EXT_COMMAND_ARG *_req_queue = (__EXT_COMMAND_ARG *)arg;
	__EXT_COMMAND_ARG *ext_arg    = NULL;
	_STAINFO          *staptr     = NULL;
/* */
	int    retry_times = 0;
	char   request[32] = { 0 };
	time_t timestamp   = 0.0;

/* */
	for ( ext_arg = _req_queue; ext_arg->chaptr != NULL; ext_arg++ ) {
		timestamp = (time_t)ext_arg->lastend;
		staptr    = ext_arg->staptr;
	/* */
		for ( ; (double)timestamp < ext_arg->starttime; timestamp++ ) {
			sprintf(request, PA2EW_EXT_RT_COMMAND_FORMAT, staptr->serial, ext_arg->chaptr->seq, timestamp);
			if ( pa2ew_server_ext_req_send( staptr, request, strlen(request) + 1 ) ) {
			/* */
				for ( retry_times = PA2EW_EXT_REQUEST_RETRY_LIMIT; retry_times > 0; retry_times-- ) {
					sleep_ew(500);
					if ( !pa2ew_server_ext_req_send( staptr, request, strlen(request) + 1 ) )
						break;
				}
			/* */
				if ( !retry_times )
					break;
			}
			sleep_ew(50);
		}
	}
/* Just exit this thread */
	free(_req_queue);
	KillSelfThread();

	return NULL;
}

/*
 *
 */
thr_ret request_soh_thread( void *dummy )
{
/* */
	pa2ew_list_walk( request_soh_stations );
/* Just exit this thread */
	KillSelfThread();

	return NULL;
}

/*
 *
 */
void process_packet_rt( PalertExtPacket *packet, _STAINFO *stainfo )
{
	int             msg_size;
	TracePacket     tracebuf;  /* message which is sent to share ring    */
	const _CHAINFO *chaptr = ((_CHAINFO *)stainfo->chaptr) + packet->rt.rt_packet.chan_seq;

/* Common part */
	enrich_trh2_rt( &tracebuf.trh2, stainfo, &packet->rt.rt_packet );
	msg_size = (tracebuf.trh2.nsamp << 2) + sizeof(TRACE2_HEADER);
/* Channel part */
	strcpy(tracebuf.trh2.chan, chaptr->chan);
	copydata_tracebuf_rt( &packet->rt.rt_packet, (int32_t *)(&tracebuf.trh2 + 1) );
/* */
	if ( tport_putmsg(&Region[EXT_MSG_LOGO], &Putlogo[WAVE_MSG_LOGO], msg_size, tracebuf.msg) != PUT_OK )
		logit("e", "palert2ew: Error putting message in region %ld\n", RingKey[EXT_MSG_LOGO]);

	return;
}

/*
 *
 */
void process_packet_soh( PalertExtPacket *packet, _STAINFO *stainfo )
{
	EXT_SOH_PACKET *ext_soh = &packet->soh.soh_packet;

/* Common part */
	logit(
		"", "%s: <sensor status: %d>, <cpu temp: %d>, <ext volt: %d>, <int volt: %d>, <rtc battery: %d>\n",
		stainfo->sta,
		ext_soh->sensor_status, ext_soh->cpu_temp, ext_soh->ext_volt, ext_soh->int_volt, ext_soh->rtc_battery
	);
	logit(
		"", "%s: <ntp status: %d>, <gnss status: %d>, <gps lock: %d>, <satellite num: %d>, "
		"<latitude: %f>, <longitude: %f>\n",
		stainfo->sta,
		ext_soh->ntp_status, ext_soh->gnss_status, ext_soh->gps_lock, ext_soh->satellite_num,
		ext_soh->latitude, ext_soh->longitude
	);
/* */
	if ( tport_putmsg(&Region[EXT_MSG_LOGO], &Putlogo[EXT_MSG_LOGO], packet->header.length, (char *)packet) != PUT_OK )
		logit("e", "palert2ew: Error putting message in region %ld\n", RingKey[EXT_MSG_LOGO]);

	return;
}

/*
 * copydata_tracebuf_rt() -
 */
int32_t *copydata_tracebuf_rt( const EXT_RT_PACKET *rt_packet, int32_t *buffer )
{
	int      i;
	int16_t *sdata_in = (int16_t *)rt_packet->data;

	switch ( rt_packet->data_bytes ) {
	case 2:
		for ( i = 0; i < rt_packet->nsamp; i++, sdata_in++, buffer++ )
			*buffer = *sdata_in;
		break;
	case 4: default:
		memcpy(buffer, rt_packet->data, rt_packet->nsamp << 2);
		break;
	}

	return buffer;
}

/*
 *
 */
__EXT_COMMAND_ARG *insert_request_queue(
	__EXT_COMMAND_ARG **queue, _STAINFO *staptr, _CHAINFO *chaptr, double lastend, double start, double end
) {
	int i;
	__EXT_COMMAND_ARG *result = NULL;

/* First time */
	if ( *queue == NULL )
		*queue = create_request_queue( staptr->nchannel );

	if ( *queue != NULL ) {
		for ( i = 0, result = *queue; i < staptr->nchannel; i++, result++ ) {
			if ( result->chaptr == NULL ) {
				enrich_ext_command_arg( result, staptr, chaptr, lastend, start, end );
				break;
			}
		}
		if ( i == staptr->nchannel )
			logit("e", "palert2ew: Too much request in the queue of station %s; skip it!\n", staptr->sta);
	}
	else {
		logit("e", "palert2ew: Error inserting the extension request for station %s; skip it!\n", staptr->sta);
	}

	return result;
}

/*
 *
 */
__EXT_COMMAND_ARG *create_request_queue( const int queue_size )
{
	int i;
	__EXT_COMMAND_ARG *result = (__EXT_COMMAND_ARG *)calloc(queue_size + 1, sizeof(__EXT_COMMAND_ARG));

	if ( result != NULL ) {
		for ( i = 0; i <= queue_size; i++ ) {
			result[i].staptr    = NULL;
			result[i].chaptr    = NULL;
			result[i].lastend   = -1.0;
			result[i].starttime = -1.0;
			result[i].endtime   = -1.0;
		}
	}

	return result;
}

/*
 *
 */
__EXT_COMMAND_ARG *enrich_ext_command_arg(
	__EXT_COMMAND_ARG *dest, _STAINFO *staptr, _CHAINFO *chaptr, double lastend, double start, double end
) {
	if ( dest != NULL ) {
		dest->staptr    = staptr;
		dest->chaptr    = chaptr;
		dest->lastend   = lastend;
		dest->starttime = start;
		dest->endtime   = end;
	}

	return dest;
}

/*
 *
 */
void request_soh_stations( const void *nodep, const VISIT which, const int depth )
{
	_STAINFO *staptr      = *(_STAINFO **)nodep;
	int       retry_times = 0;
	char      request[32] = { 0 };
	time_t    timestamp   = 0.0;

	switch ( which ) {
	case postorder: case leaf:
	/* */
		if ( staptr->ext_conn != NULL ) {
			time(&timestamp);
			sprintf(request, PA2EW_EXT_SOH_COMMAND_FORMAT, staptr->serial, timestamp);
			if ( pa2ew_server_ext_req_send( staptr, request, strlen(request) + 1 ) ) {
			/* */
				for ( retry_times = PA2EW_EXT_REQUEST_RETRY_LIMIT; retry_times > 0; retry_times-- ) {
					sleep_ew(50);
					if ( !pa2ew_server_ext_req_send( staptr, request, strlen(request) + 1 ) )
						break;
				}
			}
		}
		break;
	case preorder: case endorder:
	default:
		break;
	}

	return;
}

/*
 * enrich_trh2_rt() -
 */
TRACE2_HEADER *enrich_trh2_rt(
	TRACE2_HEADER *trh2, const _STAINFO *staptr, const EXT_RT_PACKET *rt_packet
) {
	return enrich_trh2(
		trh2, staptr->sta, staptr->net, staptr->loc, rt_packet->nsamp,
		UniSampRate ? (double)UniSampRate : (double)rt_packet->samprate,
		rt_packet->timestamp
	);
}
