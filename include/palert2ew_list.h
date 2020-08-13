/*
 *
 */
#pragma once

/* */
#include <search.h>

/* */
#include <stalist_sql.h>
#include <palert2ew.h>

/* */
int  palert2ew_list_fetch( const char *, const DBINFO * );
void palert2ew_list_end( void );
void palert2ew_list_walk( void (*)(const void *, const VISIT, const int) );
int  palert2ew_list_total_station( void );

_STAINFO *palert2ew_list_find( const int );
