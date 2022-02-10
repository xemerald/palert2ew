/*
 *
 */
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <search.h>
#include <time.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <transport.h>
#include <trace_buf.h>
/* */
#include <trh2_enrich.h>
#include <palert2ew.h>
#include <palert2ew_list.h>
#include <palert2ew_ext.h>
#include <palert2ew_server_ext.h>

/* Functions prototype in this source file
 *******************************/
static void               request_soh_stations( const void *, const VISIT, const int );
static int32_t           *copydata_tracebuf_rt( int32_t *, const EXT_RT_PACKET * );
static TRACE2_HEADER     *enrich_trh2_rt( TRACE2_HEADER *, const _STAINFO *, const EXT_RT_PACKET *, const int );
static __EXT_COMMAND_ARG *create_request_queue( const int );
static __EXT_COMMAND_ARG *enrich_ext_command_arg(
	__EXT_COMMAND_ARG *, const _STAINFO *, const _CHAINFO *, const double, const double, const double
);

/* */
#define GEN_EXT_STATUS_STRING(STATUS) \
		((STATUS) ? "OK" : "NG")

/*
 *
 */
thr_ret pa2ew_ext_rt_req_thread( void *arg )
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
thr_ret pa2ew_ext_soh_req_thread( void *dummy )
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
__EXT_COMMAND_ARG *pa2ew_ext_req_queue_insert(
	void **queue, _STAINFO *staptr, _CHAINFO *chaptr, double lastend, double start, double end
) {
	int i;
	__EXT_COMMAND_ARG **_queue = (__EXT_COMMAND_ARG **)queue;
	__EXT_COMMAND_ARG  *result = NULL;

/* First time */
	if ( *_queue == NULL )
		*_queue = create_request_queue( staptr->nchannel );

	if ( *_queue != NULL ) {
		for ( i = 0, result = *_queue; i < staptr->nchannel; i++, result++ ) {
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
int pa2ew_ext_rt_packet_process(
	void *dest, PalertExtPacket *packet, _STAINFO *stainfo, const int uni_samprate
) {
	TracePacket    *tracebuf = (TracePacket *)dest;
	const _CHAINFO *chaptr = ((_CHAINFO *)stainfo->chaptr) + packet->rt.rt_packet.chan_seq;

/* Common part */
	enrich_trh2_rt( &tracebuf->trh2, stainfo, &packet->rt.rt_packet, uni_samprate );
/* Channel part */
	strcpy(tracebuf->trh2.chan, chaptr->chan);
	copydata_tracebuf_rt( (int32_t *)(&tracebuf->trh2 + 1), &packet->rt.rt_packet );

	return (tracebuf->trh2.nsamp << 2) + sizeof(TRACE2_HEADER);
}

/*
 *
 */
int pa2ew_ext_soh_packet_process( void *dest, PalertExtPacket *packet, _STAINFO *stainfo )
{
	EXT_SOH_PACKET *ext_soh   = &packet->soh.soh_packet;
	char           *output    = (char *)dest;

	float _cpu_temp    = ext_soh->cpu_temp * PA2EW_EXT_SOH_INTVALUE_UNIT;
	float _ext_volt    = ext_soh->ext_volt * PA2EW_EXT_SOH_INTVALUE_UNIT;
	float _int_volt    = ext_soh->int_volt * PA2EW_EXT_SOH_INTVALUE_UNIT;
	float _rtc_battery = ext_soh->rtc_battery * PA2EW_EXT_SOH_INTVALUE_UNIT;

/* */
	sprintf(
		output, "#SOH:%s: <sensor status: %s>, <cpu temp: %.2f>, <ext volt: %.2f>, <int volt: %.2f>, <rtc battery: %.2f>\n"
		"#SOH:%s: <ntp status: %s>, <gnss status: %s>, <gps lock: %s>, <satellite num: %d>\n"
		"#SOH:%s: <latitude: %.6f>, <longitude: %.6f>",
		stainfo->sta, GEN_EXT_STATUS_STRING(ext_soh->sensor_status), _cpu_temp, _ext_volt, _int_volt, _rtc_battery,
		stainfo->sta, GEN_EXT_STATUS_STRING(ext_soh->ntp_status), GEN_EXT_STATUS_STRING(ext_soh->gnss_status),
		GEN_EXT_STATUS_STRING(ext_soh->gps_lock), ext_soh->satellite_num,
		stainfo->sta, ext_soh->latitude, ext_soh->longitude
	);

	return strlen(output) + 1;
}

/*
 *
 */
static void request_soh_stations( const void *nodep, const VISIT which, const int depth )
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
 * copydata_tracebuf_rt() -
 */
static int32_t *copydata_tracebuf_rt( int32_t *buffer, const EXT_RT_PACKET *rt_packet )
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
 * enrich_trh2_rt() -
 */
static TRACE2_HEADER *enrich_trh2_rt(
	TRACE2_HEADER *trh2, const _STAINFO *staptr, const EXT_RT_PACKET *rt_packet, const int uni_samprate
) {
	return trh2_enrich(
		trh2, staptr->sta, staptr->net, staptr->loc, rt_packet->nsamp,
		uni_samprate ? (double)uni_samprate : (double)rt_packet->samprate,
		rt_packet->timestamp
	);
}

/*
 *
 */
static __EXT_COMMAND_ARG *create_request_queue( const int queue_size )
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
static __EXT_COMMAND_ARG *enrich_ext_command_arg(
	__EXT_COMMAND_ARG *dest, const _STAINFO *staptr, const _CHAINFO *chaptr,
	const double lastend, const double start, const double end
) {
	if ( dest != NULL ) {
		dest->staptr    = (_STAINFO *)staptr;
		dest->chaptr    = (_CHAINFO *)chaptr;
		dest->lastend   = lastend;
		dest->starttime = start;
		dest->endtime   = end;
	}

	return dest;
}
