/**
 * @file palert2ew_misc.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for enrich tracebuf 2 header & some others.
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
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
#include <trace_buf.h>  /* For TRACE2_HEADER */

/**
 * @name Export functions' prototype
 *
 */
TRACE2_HEADER *pa2ew_trh2_init( TRACE2_HEADER * );
TRACE2_HEADER *pa2ew_trh2_scn_enrich( TRACE2_HEADER *, const char *, const char *, const char * );
TRACE2_HEADER *pa2ew_trh2_sampinfo_enrich( TRACE2_HEADER *, const int, const double, const double, const char [2] );
double  pa2ew_timenow_get( void );
int     pa2ew_recv_thrdnum_eval( int, const int );
int     pa2ew_endian_get( void );
void    pa2ew_crc8_init( void );
uint8_t pa2ew_crc8_cal( const void *, const size_t );
