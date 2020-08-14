/*
 *
 */
#pragma once

/* */
#include <search.h>

/* */
#include <stalist.h>
#include <palert2ew.h>

/* */
int   palert2ew_list_db_fetch( void **, const char *, const char *, const DBINFO * );
void  palert2ew_list_end( void );
void  palert2ew_list_walk( void (*)(const void *, const VISIT, const int) );
int   palert2ew_list_total_station( void );
_STAINFO *palert2ew_list_station_add(
	void **, const int, const char *, const char *, const char *, const int, const char *[]
);
void *palert2ew_list_root_switch( void * );

_STAINFO *palert2ew_list_find( const int );
