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
TRACE2_HEADER *pa2ew_misc_trh2_enrich(
	TRACE2_HEADER *, const char *, const char *, const char *, const int, const double, const double
);
double  pa2ew_misc_timenow_get( void );
int     pa2ew_misc_recv_thrdnum_eval( int, const int, const int );
void    pa2ew_misc_crc8_init( void );
uint8_t pa2ew_misc_crc8_cal( const void *, const size_t );
