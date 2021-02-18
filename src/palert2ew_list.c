/*
 *
 */
#define _GNU_SOURCE
/* */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
/* */
#include <earthworm.h>
/* */
#include <palert2ew_list.h>

/* Internal functions' prototypes */
static int       fetch_list_sql( void **, const char *, const char *, const DBINFO * );
static _STAINFO *add_new_station(
	void **, const int, const char *, const char *, const char *, const int, const char *[]
);
static _CHAINFO *enrich_chainfo_default( _STAINFO * );
static _STAINFO *enrich_stainfo_raw( _STAINFO *, const int, const char *, const char *, const char * );
static _CHAINFO *enrich_chainfo_raw( _STAINFO *, const int, const char *[] );
static int       compare_serial( const void *, const void * );	/* The compare function of binary tree search */
static void      cal_total_stations( const void *, const VISIT, const int );
static void      free_stainfo( void * );
#if defined( _USE_SQL )
static void extract_stainfo_mysql( int *, char *, char *, char *, const MYSQL_ROW, const unsigned long * );
static int  extract_chainfo_mysql( char *[], MYSQL_RES * );
#endif

/* Global variables */
static volatile void *Root          = NULL;       /* Root of serial binary tree */
static volatile int   TotalStations = 0;

/*
 * pa2ew_list_db_fetch() -
 */
int pa2ew_list_db_fetch( void **root, const char *table_sta, const char *table_chan, const DBINFO *dbinfo )
{
	if ( strlen(dbinfo->host) > 0 && strlen(table_sta) > 0 )
		return fetch_list_sql( root, table_sta, table_chan, dbinfo );
	else
		return 0;
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
 *
 */
int pa2ew_list_station_line_parse( void **root, const char *line )
{
	int   i, result = 0;
	int   serial;
	int   nchannel;
	char  sta[TRACE2_STA_LEN] = { 0 };
	char  net[TRACE2_NET_LEN] = { 0 };
	char  loc[TRACE2_LOC_LEN] = { 0 };
	char *chan[PALERTMODE1_CHAN_COUNT] = { NULL };
	char *sub_line = malloc(strlen(line) + 1);
	char *str_start, *str_end, *str_limit;

/* */
	if ( sscanf(line, "%d %s %s %s %d %[^\n]", &serial, sta, net, loc, &nchannel, sub_line) >= 5 ) {
		str_start = str_end = sub_line;
		str_limit = sub_line + strlen(sub_line) + 1;
		for ( i = 0; i < nchannel && i < PALERTMODE1_CHAN_COUNT; i++ ) {
		/* */
			for ( str_start = str_end; isspace(*str_start) && str_start < str_limit; str_start++ );
			for ( str_end = str_start; !isspace(*str_end) && str_end <= str_limit; str_end++ );
			*str_end++ = '\0';
		/* */
			if ( strlen(str_start) ) {
				chan[i] = malloc(strlen(str_start) + 1);
				strcpy(chan[i], str_start);
			}
			else {
				logit("e", "palert2ew: ERROR, lack of channel code for station %s in local list!\n", sta);
				result = -1;
				break;
			}
		}
	/* */
		if ( result != -1 )
			if ( add_new_station( root, serial, sta, net, loc, i, (const char **)chan ) == NULL )
				result = -2;
	}
	else {
		logit("e", "palert2ew: ERROR, lack of some station information for serial %d in local list!\n", serial);
		result = -1;
	}
/* */
	for ( i = 0; i < nchannel && i < PALERTMODE1_CHAN_COUNT; i++ )
		free(chan[i]);
/* */
	free(sub_line);

	return result;
}

/*
 * pa2ew_list_root_reg() -
 */
void *pa2ew_list_root_reg( void *root )
{
	void *_root = (void *)Root;

/* Switch the tree's root */
	Root = root;
/* Free the old one */
	sleep_ew(500);
	pa2ew_list_root_destroy( _root );

	return (void *)Root;
}

/*
 * pa2ew_list_root_destroy() -
 */
void pa2ew_list_root_destroy( void *root )
{
	if ( root != (void *)NULL )
		tdestroy(root, free_stainfo);

	return;
}

#if defined( _USE_SQL )
/*
 * fetch_list_sql() - Get stations list from MySQL server
 */
static int fetch_list_sql( void **root, const char *table_sta, const char *table_chan, const DBINFO *dbinfo )
{
	int   i, result = 0;
	int   serial;
	int   nchannel;
	char  sta[TRACE2_STA_LEN] = { 0 };
	char  net[TRACE2_NET_LEN] = { 0 };
	char  loc[TRACE2_LOC_LEN] = { 0 };
	char *chan[PALERTMODE1_CHAN_COUNT] = { NULL };

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
/* */
	for ( i = 0; i < PALERTMODE1_CHAN_COUNT; i++ )
		chan[i] = (char *)malloc(TRACE2_CHAN_LEN);

/* Start the SQL server connection for channel */
	stalist_start_persistent_sql( dbinfo );
/* Read station list from query result */
	while ( (sql_row = stalist_fetch_row_sql( sql_res )) != NULL ) {
	/* */
		extract_stainfo_mysql( &serial, sta, net, loc, sql_row, stalist_fetch_lengths_sql( sql_res ) );
	/* */
		nchannel = 0;
		if ( table_chan != NULL && strlen(table_chan) )
			nchannel = extract_chainfo_mysql(
				chan, stalist_chan_query_sql( dbinfo, table_chan, sta, net, loc, 1, COL_CHAN_CHANNEL )
			);
	/* */
		if ( add_new_station( root, serial, sta, net, loc, nchannel, (const char **)chan ) != NULL ) {
			result++;
		}
		else {
			result = -2;
			break;
		}
	}
/* Close the connection for channel */
	stalist_close_persistent_sql();
	stalist_free_result_sql( sql_res );

	if ( result > 0 ) {
		logit("o", "palert2ew: Read %d stations information from MySQL server success!\n", result);
	}
	else {
		logit("e", "palert2ew: Some errors happened when fetching station information from MySQL server!\n");
	}
/* */
	for ( i = 0; i < PALERTMODE1_CHAN_COUNT; i++ )
		free(chan[i]);

	return result;
}

/*
 * extract_stainfo_mysql() -
 */
static void extract_stainfo_mysql(
	int *serial, char *sta, char *net, char *loc,
	const MYSQL_ROW sql_row, const unsigned long *row_lengths
) {
	char _str[16] = { 0 };

/* */
	*serial = atoi(stalist_field_extract_sql( _str, sizeof(_str), sql_row[0], row_lengths[0] ));
	stalist_field_extract_sql( sta, TRACE2_STA_LEN, sql_row[1], row_lengths[1] );
	stalist_field_extract_sql( net, TRACE2_NET_LEN, sql_row[2], row_lengths[2] );
	stalist_field_extract_sql( loc, TRACE2_LOC_LEN, sql_row[3], row_lengths[3] );

	return;
}

/*
 * extract_chainfo_mysql() -
 */
static int extract_chainfo_mysql( char *chan[], MYSQL_RES *sql_res )
{
/* */
	int i, result = 0;
	MYSQL_ROW sql_row;
	unsigned long *row_lengths;

/* */
	if ( sql_res != NULL ) {
	/* */
		i = 0;
		result = stalist_num_rows_sql( sql_res );
		if ( result > 0 && result <= PALERTMODE1_CHAN_COUNT ) {
			while ( (sql_row = stalist_fetch_row_sql( sql_res )) != NULL ) {
				row_lengths = stalist_fetch_lengths_sql( sql_res );
				stalist_field_extract_sql( chan[i++], TRACE2_LOC_LEN, sql_row[0], row_lengths[0] );
			}
		}
		stalist_free_result_sql( sql_res );
	}

	return result;
}
#else
/*
 * fetch_list_sql() - Fake function
 */
static int fetch_list_sql( void **root, const char *table_sta, const char *table_chan, const DBINFO *dbinfo )
{
	printf(
		"palert2ew: Skip the process of fetching station list from remote database "
		"'cause you did not define the _USE_SQL tag when compiling.\n"
	);
	return 0;
}
#endif

/*
 * add_new_station() -
 */
static _STAINFO *add_new_station(
	void **root, const int serial, const char *sta, const char *net,
	const char *loc, const int nchannel, const char *chan[]
) {
	_STAINFO *stainfo = (_STAINFO *)calloc(1, sizeof(_STAINFO));
	_STAINFO *result  = NULL;

	if ( stainfo != NULL ) {
		enrich_stainfo_raw( stainfo, serial, sta, net, loc );
	/* */
		if ( (result = tfind(stainfo, root, compare_serial)) == NULL ) {
		/* */
			stainfo->chaptr = NULL;
			if ( nchannel )
				enrich_chainfo_raw( stainfo, nchannel, chan );
			else
				enrich_chainfo_default( stainfo );
		/* Insert the station information into binary tree */
			if ( stainfo->chaptr != NULL ) {
				if ( tsearch(stainfo, root, compare_serial) != NULL )
					return stainfo;
				else
					logit("e", "palert2ew: Error insert station into binary tree!\n");
			}
			else {
				logit("e", "palert2ew: Error allocate the memory for channels information!\n");
			}
		}
		else {
			logit("o", "palert2ew: Serial %d is already in the list, skip it!\n", serial);
			result = *(_STAINFO **)result;
		}
	/* */
		free(stainfo);
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
	stainfo->normal_conn = NULL;
	stainfo->ext_conn    = NULL;

	return stainfo;
}

/*
 *
 */
static _CHAINFO *enrich_chainfo_raw( _STAINFO *stainfo, const int nchannel, const char *chan[] )
{
	int       i;
	_CHAINFO *chainfo = calloc(nchannel, sizeof(_CHAINFO));

/* */
	stainfo->nchannel = nchannel;
	stainfo->chaptr   = chainfo;

	if ( chainfo != NULL ) {
		for ( i = 0; i < nchannel; i++ ) {
			chainfo[i].seq = i;
			strcpy(chainfo[i].chan, chan[i]);
			chainfo[i].last_endtime = -1.0;
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
