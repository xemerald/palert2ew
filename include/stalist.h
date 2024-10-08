/**
 * @file stalist.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for parse station list data.
 * @date 2020-03-01
 *
 * @copyright Copyright (c) 2020
 *
 */

#pragma once

/**
 * @name
 *
 */
#include <mysql.h>

/**
 * @name
 *
 */
#include <dbinfo.h>

/**
 * @brief
 *
 */
#define COL_STA_LIST_TABLE \
		X(COL_STA_SERIAL,   "serial"  ) \
		X(COL_STA_STATION,  "station" ) \
		X(COL_STA_NETWORK,  "network" ) \
		X(COL_STA_LOCATION, "location") \
		X(COL_STA_LIST_COUNT, "NULL"  )

#define X(a, b) a,
typedef enum {
	COL_STA_LIST_TABLE
} COL_STA_LIST;
#undef X

/**
 * @brief
 *
 */
#define COL_CHAN_LIST_TABLE \
		X(COL_CHAN_CHANNEL,  "channel"          ) \
		X(COL_CHAN_SEQ,      "sequence"         ) \
		X(COL_CHAN_RECORD,   "record_type"      ) \
		X(COL_CHAN_INST,     "instrument"       ) \
		X(COL_CHAN_SAMPRATE, "samprate"         ) \
		X(COL_CHAN_CFACTOR,  "conversion_factor") \
		X(COL_CHAN_LIST_COUNT, "NULL"           )

#define X(a, b) a,
typedef enum {
	COL_CHAN_LIST_TABLE
} COL_CHAN_LIST;
#undef X

/**
 * @brief
 *
 */
typedef union {
	COL_STA_LIST  col_sta;
	COL_CHAN_LIST col_chan;
} STALIST_COL_LIST __attribute__((__transparent_union__));

/**
 * @brief
 *
 */
typedef char *(*GET_COLUMN_NAME)( const STALIST_COL_LIST );

/**
 * @name Export functions' prototypes
 *
 */
MYSQL_RES *stalist_sta_query_sql( const DBINFO *, const char *, const int, ... );
MYSQL_RES *stalist_chan_query_sql(
	const DBINFO *, const char *, const char *, const char *, const char *, const int, ...
);
MYSQL_ROW      stalist_fetch_row_sql( MYSQL_RES * );
unsigned long *stalist_fetch_lengths_sql( MYSQL_RES * );
int            stalist_num_rows_sql( MYSQL_RES * );
unsigned int   stalist_num_fields_sql( MYSQL_RES * );
char          *stalist_field_extract_sql( char *, const unsigned int, const void *, const unsigned int );
void           stalist_free_result_sql( MYSQL_RES * );
MYSQL         *stalist_start_persistent_sql( const DBINFO * );
void           stalist_close_persistent_sql( void );
void           stalist_end_thread_sql( void );
