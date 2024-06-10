/*
 * palert2ew_misc.h
 *
 * Header file for enrich tracebuf 2 header.
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
/* For TRACE2_HEADER */
#include <trace_buf.h>

/* Function prototype */
TRACE2_HEADER *pa2ew_trh2_init( TRACE2_HEADER * );
TRACE2_HEADER *pa2ew_trh2_scn_enrich( TRACE2_HEADER *, const char *, const char *, const char * );
TRACE2_HEADER *pa2ew_trh2_sampinfo_enrich( TRACE2_HEADER *, const int, const double, const double, const char [2] );
double  pa2ew_timenow_get( void );
int     pa2ew_recv_thrdnum_eval( int, const int );
int     pa2ew_endian_get( void );
void    pa2ew_crc8_init( void );
uint8_t pa2ew_crc8_cal( const void *, const size_t );
