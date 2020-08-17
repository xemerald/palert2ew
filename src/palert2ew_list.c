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
static int       fetch_list_sql( void **, const char *, const char *, const DBINFO * );
static _STAINFO *enrich_stainfo_mysql( _STAINFO *, const MYSQL_ROW );
static _CHAINFO *enrich_chainfo_mysql( _STAINFO *, MYSQL_RES * );
static _CHAINFO *enrich_chainfo_default( _STAINFO * );
static _STAINFO *enrich_stainfo_raw( _STAINFO *, const int, const char *, const char *, const char * );
static _CHAINFO *enrich_chainfo_raw( _STAINFO *, const int, const char *[] );
static int       compare_serial( const void *, const void * );	/* The compare function of binary tree search */
static void      cal_total_stations( const void *, const VISIT, const int );
static void      free_stainfo( void * );

/* Global variables */
static volatile void *Root          = NULL;       /* Root of serial binary tree */
static volatile int   TotalStations = 0;

/*
 * pa2ew_list_db_fetch() -
 */
int pa2ew_list_db_fetch( void **root, const char *table_sta, const char *table_chan, const DBINFO *dbinfo )
{
	return fetch_list_sql( root, table_sta, table_chan, dbinfo );
}

/*
 * pa2ew_list_serial_find() -
 */
_STAINFO *pa2ew_list_find( const int serial )
{
	_STAINFO *result = NULL;
	_STAINFO  key;

/* */
	key.serial = serial;
/* Find which station */
	if ( (result = tfind(&key, (void **)&Root, compare_serial)) == NULL ) {
	/* Not found in Palert table */
		return NULL;
	}
	result = *(_STAINFO **)result;

	return result;
}

/*
 * pa2ew_list_end() -
 */
void pa2ew_list_end( void )
{
	tdestroy((void *)Root, free_stainfo);

	return;
}

/*
 * pa2ew_list_walk() -
 */
void pa2ew_list_walk( void (*action)(const void *, const VISIT, const int) )
{
	twalk((void *)Root, action);
	return;
}

/*
 * pa2ew_list_total_station() -
 */
int pa2ew_list_total_station( void )
{
	TotalStations = 0;
	pa2ew_list_walk( cal_total_stations );

	return TotalStations;
}

/*
 * pa2ew_list_station_add() -
 */
_STAINFO *pa2ew_list_station_add(
	void **root, const int serial, const char *sta, const char *net, const char *loc, const int nchannel, const char *chan[]
) {
	_STAINFO *result = (_STAINFO *)calloc(1, sizeof(_STAINFO));

	if ( result != NULL ) {
		enrich_stainfo_raw( result, serial, sta, net, loc );
		if ( tfind(result, root, compare_serial) == NULL ) {
		/* */
			if ( nchannel )
				enrich_chainfo_raw( result, nchannel, chan );
			else
				enrich_chainfo_default( result );

		/* Insert the station information into binary tree */
			if ( result->chaptr != NULL ) {
				if ( tsearch(result, root, compare_serial) != NULL )
					return result;
				else
					logit("e", "palert2ew: Error insert station into binary tree!\n");
			}
			else {
				logit("e", "palert2ew: Error allocate the memory for channels information!\n");
			}
		}
		free(result);
		result = NULL;
	}

	return result;
}

/*
 * pa2ew_list_root_switch() -
 */
void *pa2ew_list_root_switch( void **root )
{
	void *_root = (void *)Root;

/* Switch the tree's root */
	Root = *root;
/* Free the old one */
	sleep_ew(500);
	if ( _root != (void *)NULL ) tdestroy(_root, free_stainfo);
	*root = NULL;

	return (void *)Root;
}

/*
 * fetch_list_sql() - Get stations list from MySQL server
 */
static int fetch_list_sql( void **root, const char *table_sta, const char *table_chan, const DBINFO *dbinfo )
{
	int        result  = 0;
	_STAINFO  *stainfo = NULL;
	MYSQL_RES *sql_res = NULL;
	MYSQL_ROW  sql_row;

/* Connect to database */
	printf("palert2ew: Querying the station information from MySQL server %s...\n", dbinfo->host);
	sql_res = stalist_sta_query_sql(
		dbinfo, table_sta, PA2EW_INFO_FROM_SQL,
		COL_STA_SERIAL, COL_STA_STATION, COL_STA_NETWORK, COL_STA_LOCATION
	);
	if ( sql_res == NULL )
		return -1;
	printf("palert2ew: Queried the station information success!\n");

/* Start the SQL server connection for channel */
	stalist_start_persistent_sql( dbinfo );
/* Read station list from query result */
	while ( (sql_row = stalist_fetch_row_sql(sql_res)) != NULL ) {
	/* Allocate the station information memory */
		if ( (stainfo = (_STAINFO *)calloc(1, sizeof(_STAINFO))) != NULL ) {
			enrich_stainfo_mysql( stainfo, sql_row );
			if ( tfind(stainfo, root, compare_serial) == NULL ) {
				if ( table_chan != NULL && strlen(table_chan) ) {
					enrich_chainfo_mysql(
						stainfo,
						stalist_chan_query_sql(
							dbinfo, table_chan, stainfo->sta, stainfo->net, stainfo->loc, 1, COL_CHAN_CHANNEL
						)
					);
				}
				else {
					enrich_chainfo_default( stainfo );
				}
			/* */
				if ( stainfo->chaptr != NULL ) {
				/* Insert the station information into binary tree */
					if ( tsearch(stainfo, root, compare_serial) != NULL ) {
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
				free(stainfo);
				stainfo = NULL;
				continue;
			}
		}
		else {
			logit("e", "palert2ew: Error allocate the memory for station information!\n");
		}
		result = -2;
		break;
	}
/* Close the connection for channel */
	stalist_close_persistent_sql();
	stalist_free_result_sql(sql_res);

	if ( result > 0 ) {
		logit("o", "palert2ew: Read %d stations information from MySQL server success!\n", result);
	}
	else {
		if ( stainfo != (_STAINFO *)NULL )
			free(stainfo);
		if ( *root != (void *)NULL )
			tdestroy(*root, free_stainfo);
	}

	return result;
}

/*
 *
 */
static _STAINFO *enrich_stainfo_mysql( _STAINFO *stainfo, const MYSQL_ROW sql_row )
{
	return enrich_stainfo_raw(
		stainfo,
		atoi(sql_row[COL_STA_SERIAL]),
		sql_row[COL_STA_STATION],
		sql_row[COL_STA_NETWORK],
		sql_row[COL_STA_LOCATION]
	);
}

/*
 *
 */
static _CHAINFO *enrich_chainfo_mysql( _STAINFO *stainfo, MYSQL_RES *sql_res )
{
/* */
	if ( sql_res == NULL )
		return NULL;
/* */
	int       i = 0;
	const int nchannel = stalist_num_rows_sql( sql_res );
	char    **chan = NULL;

	MYSQL_ROW sql_row;
	_CHAINFO *result = NULL;

/* */
	if ( nchannel > 0 ) {
		chan = calloc(nchannel, sizeof(char *));
		while ( (sql_row = stalist_fetch_row_sql( sql_res )) != NULL ) {
			chan[i] = malloc(TRACE2_CHAN_LEN);
			strcpy(chan[i++], sql_row[0]);
		}
		stalist_free_result_sql( sql_res );

		result = enrich_chainfo_raw( stainfo, nchannel, (const char **)chan );
	/* */
		for ( i = 0; i < nchannel; i++ )
			free(chan[i]);
		free(chan);
	}

	return result;
}

/*
 *
 */
static _CHAINFO *enrich_chainfo_default( _STAINFO *stainfo )
{
#define X(a, b, c) b,
	const char *chan[] = {
		PALERTMODE1_CHAN_TABLE
	};
#undef X

	return enrich_chainfo_raw( stainfo, PA2EW_DEF_CHAN_PER_STA, chan );
}

/*
 *
 */
static _STAINFO *enrich_stainfo_raw(
	_STAINFO *stainfo, const int serial, const char *sta, const char *net, const char *loc
) {
/* */
	stainfo->serial = serial;
	strcpy(stainfo->sta, sta);
	strcpy(stainfo->net, net);
	strcpy(stainfo->loc, loc);
	stainfo->packet.sptr = stainfo;

	return stainfo;
}

/*
 *
 */
static _CHAINFO *enrich_chainfo_raw(
	_STAINFO *stainfo, const int nchannel, const char *chan[]
) {
	int       i;
	_CHAINFO *chainfo = NULL;

/* */
	stainfo->nchannel = nchannel;
	stainfo->chaptr   = calloc(stainfo->nchannel, sizeof(_CHAINFO));
	chainfo           = (_CHAINFO *)stainfo->chaptr;

	if ( chainfo != NULL ) {
		for ( i = 0; i < stainfo->nchannel; i++ ) {
			chainfo[i].seq = i;
			strcpy(chainfo[i].chan, chan[i]);
		}
	}

	return chainfo;
}

/*
 *
 */
static void cal_total_stations( const void *nodep, const VISIT which, const int depth )
{
	switch ( which ) {
	case postorder:
	case leaf:
		TotalStations++;
		break;
	case preorder:
	case endorder:
	default:
		break;
	}

	return;
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
 * free_stainfo() -
 */
static void free_stainfo( void *node )
{
	_STAINFO *stainfo = (_STAINFO *)node;

	free(stainfo->chaptr);
	free(stainfo);

	return;
}
