/*
 * stalist.h
 *
 * Header file for parse station list data.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * March, 2020
 *
 */

#pragma once
/* */
#include <mysql.h>

/* Define the character length of parameters */
#define  MAX_HOST_LENGTH      256
#define  MAX_USER_LENGTH      16
#define  MAX_PASSWORD_LENGTH  32
#define  MAX_DATABASE_LENGTH  64
#define  MAX_TABLE_LEGTH      64

/* */
#define STA_CODE_LEN   8     /* 5 bytes plus 3 bytes padding */

/* Database login information */
typedef struct {
	char host[MAX_HOST_LENGTH];
	char user[MAX_USER_LENGTH];
	char password[MAX_PASSWORD_LENGTH];
	char database[MAX_DATABASE_LENGTH];
	long port;
} DBINFO;

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

/*
 *
 */
typedef union {
	COL_STA_LIST  col_sta;
	COL_CHAN_LIST col_chan;
} STALIST_COL_LIST __attribute__((__transparent_union__));

typedef char *(*GET_COLUMN_NAME)( const STALIST_COL_LIST );

/* Export functions' prototypes */
MYSQL_RES *stalist_sta_query_sql( const DBINFO *, const char *, const int, ... );
MYSQL_RES *stalist_chan_query_sql(
	const DBINFO *, const char *, const char *, const char *, const char *, const int, ...
);
MYSQL_ROW  stalist_fetch_row_sql( MYSQL_RES * );
int        stalist_num_rows_sql( MYSQL_RES * );
void       stalist_free_result_sql( MYSQL_RES * );
MYSQL     *stalist_start_persistent_sql( const DBINFO * );
void       stalist_close_persistent_sql( void );

/* */
char      *stalist_get_chan_code( const int );
int        stalist_get_chan_seq( const char * );
char      *stalist_get_chan_code( const int );
char      *stalist_get_chan_recordtype( const int );
char      *stalist_get_chan_instrument( const int );
int        stalist_get_chan_samprate( const int );
double     stalist_get_chan_conv_factor( const int );
