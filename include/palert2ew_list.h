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
#if defined( _USE_SQL )
#include <stalist.h>
#else
#include <dbinfo.h>
#endif
/* */
#include <palert2ew.h>
/* */
#define PA2EW_LIST_INITIALIZING  0
#define PA2EW_LIST_UPDATING      1
/* */
#define PA2EW_PALERT_INFO_OBSOLETE 0
#define PA2EW_PALERT_INFO_UPDATED  1
/* */
int       pa2ew_list_db_fetch( const char *, const char *, const DBINFO *, const int );
int       pa2ew_list_station_line_parse( const char *, const int );
void      pa2ew_list_end( void );
_STAINFO *pa2ew_list_find( const int );
void      pa2ew_list_update_status_set( const int );
void      pa2ew_list_obsolete_clear( void );
void      pa2ew_list_tree_activate( void );
void      pa2ew_list_tree_abandon( void );
int       pa2ew_list_total_station_get( void );
double    pa2ew_list_timestamp_get( void );
void      pa2ew_list_walk( void (*)(const void *, const int, void *), void * );
