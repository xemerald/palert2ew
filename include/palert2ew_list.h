/*
 * palert2ew_list.h
 *
 * Header file for construct the dictionary of station.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * August, 2020
 *
 */
#pragma once
/* */
#include <search.h>
/* */
#include <stalist.h>
#include <palert2ew.h>

/* */
int   pa2ew_list_db_fetch( void **, const char *, const char *, const DBINFO * );
void  pa2ew_list_end( void );
void  pa2ew_list_walk( void (*)(const void *, const VISIT, const int) );
int   pa2ew_list_total_station( void );
_STAINFO *pa2ew_list_station_add(
	void **, const int, const char *, const char *, const char *, const int, const char *[]
);
void *pa2ew_list_root_switch( void ** );

_STAINFO *pa2ew_list_find( const int );
