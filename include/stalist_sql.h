/*
 * tra_stalist.h
 *
 * Header file for TRA station list data.
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
/* Serial number of station */
		X(COL_STA_SERIAL,    "serial") \
/* Station code */
		X(COL_STA_STATION,  "station") \
/* Should always be the last */
		X(COL_STA_LIST_COUNT), "NULL")

#define X(a, b) a,
typedef enum {
	COL_STA_LIST_TABLE
} COL_STA_LIST;
#undef X

/* Export functions' prototypes */
MYSQL_RES *stalist_query_sql( const DBINFO *, const char *, const int, ... );
MYSQL_ROW  stalist_fetch_row_sql( MYSQL_RES * );
void       stalist_end_sql( MYSQL_RES * );
char      *stalist_get_chan_code( const int );
int        stalist_get_chan_seq( const char * );
char      *stalist_get_chan_code( const int );
char      *stalist_get_chan_recordtype( const int );
char      *stalist_get_chan_instrument( const int );
int        stalist_get_chan_samprate( const int );
double     stalist_get_chan_conv_factor( const int );
