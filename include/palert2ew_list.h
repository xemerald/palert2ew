/**
 * @file palert2ew_list.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for construct the dictionary of station.
 * @date 2020-08-01
 *
 * @copyright Copyright (c) 2020
 *
 */


#pragma once

/**
 * @name Local header include
 *
 */
#if defined( _USE_SQL )
#include <stalist.h>
#else
#include <dbinfo.h>
#endif
#include <palert2ew.h>

/**
 * @brief
 *
 */
#define PA2EW_LIST_INITIALIZING  0
#define PA2EW_LIST_UPDATING      1

/**
 * @brief
 *
 */
#define PA2EW_PALERT_INFO_OBSOLETE 0
#define PA2EW_PALERT_INFO_UPDATED  1

/**
 * @name Export functions' prototype
 *
 */
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
void      pa2ew_list_walk( void (*)(void *, const int, void *), void * );
