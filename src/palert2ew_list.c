#define _GNU_SOURCE
/* */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <unistd.h>
#include <errno.h>

/* */
#include <earthworm.h>

/* */
#include <palert2ew_list.h>

/* Internal functions' prototypes */
static int       fetch_list_sql( const char *, const DBINFO * );
static _STAINFO *enrich_stainfo_by_row( _STAINFO *, const MYSQL_ROW );
static _CHAINFO *enrich_chainfo_by_default( _STAINFO * );
static int       compare_serial( const void *, const void * );	/* The compare function of binary tree search */
static void     *switch_root( void * );
static void      free_stainfo( void * );

/* Global variables */
static void        *Root          = NULL;       /* Root of serial binary tree */
static volatile int TotalStations = 0;

/*
 * palert2ew_list_fetch() -
 */
int palert2ew_list_fetch( const char *list_name, const DBINFO *dbinfo )
{
	return fetch_list_sql( list_name, dbinfo );
}

/*
 * palert2ew_list_serial_find() -
 */
_STAINFO *palert2ew_list_find( const int serial )
{
	_STAINFO *result = NULL;
	_STAINFO  key;

/* */
	key.serial = serial;
/* Find which station */
	if ( (result = tfind(&key, &Root, compare_serial)) == NULL ) {
	/* Not found in Palert table */
		return NULL;
	}
	result = *(_STAINFO **)result;

	return result;
}

/*
 * palert2ew_list_end() -
 */
void palert2ew_list_end( void )
{
	tdestroy(Root, free_stainfo);

	return;
}

/*
 * palert2ew_list_walk() -
 */
void palert2ew_list_walk( void (*action)(const void *, const VISIT, const int) )
{
	twalk(Root, action);
	return;
}

/*
 * palert2ew_list_total_station() -
 */
int palert2ew_list_total_station( void )
{
	return TotalStations;
}

/*
 * fetch_list_sql() - Get stations list from MySQL server
 */
static int fetch_list_sql( const char *tablename, const DBINFO *dbinfo )
{
	int        result  = 0;
	_STAINFO  *stainfo = NULL;
	void      *root    = NULL;
	MYSQL_RES *sql_res = NULL;
	MYSQL_ROW  sql_row;

/* Connect to database */
	printf("palert2ew: Connecting to MySQL server %s...\n", dbinfo->host);
	sql_res = stalist_query_sql( dbinfo, tablename, PALERT2EW_INFO_FROM_SQL, COL_STA_SERIAL, COL_STA_STATION );
	if ( sql_res == NULL )
		return -1;

	printf("palert2ew: Connected to MySQL server %s success!\n", dbinfo->host);

/* Read station list from query result */
	while ( (sql_row = stalist_fetch_row_sql(sql_res)) != NULL ) {
	/* Allocate the station information memory */
		if ( (stainfo = (_STAINFO *)calloc(1, sizeof(_STAINFO))) != NULL ) {
			enrich_stainfo_by_row( stainfo, sql_row );
			enrich_chainfo_by_default( stainfo )
			if ( stainfo->chaptr != NULL ) {
			/* Insert the station information into binary tree */
				if ( tsearch(stainfo, &root, compare_serial) != NULL ) {
				/* Counting the total number of stations & go for next row */
					stainfo = NULL;
					result++;
					continue;
				}
				else {
					logit("e", "palert2ew: Error insert station into binary tree!\n");
				}
			}
			else {
				logit("e", "palert2ew: Error allocate the memory for channels information!\n");
			}
		}
		else {
			logit("e", "palert2ew: Error allocate the memory for station information!\n");
		}
		result = -2;
		break;
	}
/* Close connection */
	stalist_end_sql(sql_res);

	if ( result > 0 ) {
		TotalStations = result;
		switch_root( root );
		logit("o", "palert2ew: Read %hd stations information from MySQL server success!\n", result);
	}
	else {
		if ( stainfo != (_STAINFO *)NULL )
			free(stainfo);
		if ( root != (void *)NULL )
			tdestroy(root, free_stainfo);
	}

	return result;
}

/*
 *
 */
static _STAINFO *enrich_stainfo_by_row( _STAINFO *stainfo, const MYSQL_ROW sql_row )
{
/* */
	stainfo->serial = atoi(sql_row[0]);
	strcpy(stainfo->sta, sql_row[1]);
	strcpy(stainfo->net, sql_row[2]);
	strcpy(stainfo->loc, sql_row[3]);
	stainfo->packet.sptr = stainfo;

	return stainfo;
}

/*
 *
 */
static _CHAINFO *enrich_chainfo_by_default( _STAINFO *stainfo )
{
	int       i;
	_CHAINFO *chainfo = NULL;

/* */
	stainfo->nchannels = PALERT2EW_DEF_CHAN_PER_STA;
	stainfo->chanptr   = calloc(stainfo->nchannels, sizeof(_CHAINFO));
	chainfo            = (_CHAINFO *)stainfo->chanptr;

	if ( chainfo != NULL ) {
		for ( i = 0; i < stainfo->nchannels; i++ ) {
			chainfo[i].seq = i;
			strcpy(chainfo[i].chan, stalist_get_chan_code( i ));
		}
	}

	return chainfo;
}

/*
 * compare_serial() -
 */
static int compare_serial( const void *node_a, const void *node_b )
{
	int serial_a = ((_STAINFO *)node_a)->serial;
	int serial_b = ((_STAINFO *)node_b)->serial;

	if ( serial_a > serial_b )
		return 1;
	else if ( serial_a < serial_b )
		return -1;
	else
		return 0;
}

/*
 *
 */
static void *switch_root( void *root )
{
	void *_root = Root;

/* Switch the tree's root */
	Root = root;
/* Free the old one */
	sleep_ew(500);
	if ( _root != (void *)NULL ) tdestroy(_root, free_stainfo);

	return Root;
}

/*
 * free_stainfo() -
 */
static void free_stainfo( void *node )
{
	_STAINFO *stainfo = (_STAINFO *)node;

	free(stainfo->chanptr);
	free(stainfo);

	return;
}
