/**
 * @file palert2ew_list.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief
 * @date 2020-08-01
 *
 * @copyright Copyright (c) 2020
 *
 */

#define _GNU_SOURCE

/**
 * @name Standard C header include
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <search.h>
#include <ctype.h>

/**
 * @name Earthworm environment header include
 *
 */
#include <earthworm.h>

/**
 * @name Local header include
 *
 */
#include <libpalertc/libpalertc.h>
#include <dl_chain_list.h>
#include <palert2ew_misc.h>
#include <palert2ew_list.h>

/**
 * @brief
 *
 */
typedef struct {
	int     count;      /* Number of clients in the list */
	double  timestamp;  /* Time of the last time updated */
	void   *entry;      /* Pointer to first client       */
	void   *root;       /* Root of binary searching tree */
	void   *root_t;     /* Temporary root of binary searching tree */
} StaList;

/**
 * @name Internal functions' prototype
 *
 */
static int       fetch_list_sql( const char *, const char *, const DBINFO *, const int );
static StaList  *init_sta_list( void );
static void      destroy_sta_list( StaList * );
static _STAINFO *append_stainfo_list( StaList *, _STAINFO *, const int );
static _STAINFO *create_new_stainfo( const int, const char *, const char *, const char *, const int, const char *[] );
static _CHAINFO *enrich_chainfo_default( _STAINFO * );
static _STAINFO *enrich_stainfo_raw( _STAINFO *, const int, const char *, const char *, const char * );
static _CHAINFO *enrich_chainfo_raw( _STAINFO *, const int, const char *[] );
static _STAINFO *update_stainfo_and_chainfo( _STAINFO *, const _STAINFO * );
static int       obsolete_clear_cond( void *, void * );
static int       compare_serial( const void *, const void * );	/* The compare function of binary tree search */
static void      dummy_func( void * );
static void      free_stainfo_and_chainfo( void * );
/* */
#if defined( _USE_SQL )
static void extract_stainfo_mysql( int *, char *, char *, char *, const MYSQL_ROW, const unsigned long * );
static int  extract_chainfo_mysql( char *[], MYSQL_RES * );
#endif

/**
 * @name Internal static variables
 *
 */
static StaList *SList = NULL;

/**
 * @brief
 *
 * @param table_sta
 * @param table_chan
 * @param dbinfo
 * @param update
 * @return int
 */
int pa2ew_list_db_fetch( const char *table_sta, const char *table_chan, const DBINFO *dbinfo, const int update )
{
	if ( !SList ) {
		SList = init_sta_list();
		if ( !SList ) {
			logit("e", "palert2ew: Fatal! Station list memory initialized error!\n");
			return -3;
		}
	}

	if ( strlen(dbinfo->host) > 0 && strlen(table_sta) > 0 )
		return fetch_list_sql( table_sta, table_chan, dbinfo, update );
	else
		return 0;
}

/**
 * @brief
 *
 * @param line
 * @param update
 * @return int
 */
int pa2ew_list_station_line_parse( const char *line, const int update )
{
	int   result = 0;
	int   serial;
	int   nchannel;
	char  sta[TRACE2_STA_LEN] = { 0 };
	char  net[TRACE2_NET_LEN] = { 0 };
	char  loc[TRACE2_LOC_LEN] = { 0 };
	char *chan[PA2EW_MAX_CHAN_PER_STA] = { NULL };
	char *sub_line = malloc(strlen(line) + 1);
	char *str_start, *str_end, *str_limit;

/* */
	if ( !SList ) {
		SList = init_sta_list();
		if ( !SList ) {
			logit("e", "palert2ew: Fatal! Station list memory initialized error!\n");
			return -3;
		}
	}
/* */
	for ( int i = 0; i < PA2EW_MAX_CHAN_PER_STA; i++ )
		chan[i] = calloc(1, TRACE2_CHAN_LEN);
/* */
	if ( sscanf(line, "%d %s %s %s %d %[^\n]", &serial, sta, net, loc, &nchannel, sub_line) >= 5 ) {
		str_start = str_end = sub_line;
		str_limit = sub_line + strlen(sub_line) + 1;
		nchannel = nchannel > PA2EW_MAX_CHAN_PER_STA ? PA2EW_MAX_CHAN_PER_STA : nchannel;
		for ( int i = 0; i < nchannel; i++ ) {
		/* */
			for ( str_start = str_end; isspace(*str_start) && str_start < str_limit; str_start++ );
			for ( str_end = str_start; !isspace(*str_end) && str_end <= str_limit; str_end++ );
			*str_end++ = '\0';
		/* */
			if ( strlen(str_start) ) {
				strncpy(chan[i], str_start, TRACE2_CHAN_LEN);
				chan[i][TRACE2_CHAN_LEN - 1] = '\0';
			}
			else {
				logit("e", "palert2ew: ERROR, lack of channel code for station %s in local list!\n", sta);
				result = -1;
				break;
			}
		}
	/* */
		if ( result != -1 ) {
			if (
				append_stainfo_list(
					SList,
					create_new_stainfo( serial, sta, net, loc, nchannel, (const char **)chan ),
					update
				) == NULL
			) {
				result = -2;
			}
		}
	}
	else {
		logit("e", "palert2ew: ERROR, lack of some station information for serial %d in local list!\n", serial);
		result = -1;
	}
/* */
	for ( int i = 0; i < PA2EW_MAX_CHAN_PER_STA; i++ )
		free(chan[i]);
/* */
	free(sub_line);

	return result;
}

/**
 * @brief
 *
 */
void pa2ew_list_end( void )
{
	destroy_sta_list( SList );
	SList = NULL;

	return;
}

/**
 * @brief
 *
 * @param serial
 * @return _STAINFO*
 */
_STAINFO *pa2ew_list_find( const int serial )
{
	_STAINFO *result = NULL;
	_STAINFO  key;

/* */
	key.serial = serial;
/* Find which station */
	if ( (result = tfind(&key, &SList->root, compare_serial)) != NULL )
	/* Found in the main Palert table */
		result = *(_STAINFO **)result;

	return result;
}

/**
 * @brief
 *
 * @param update_status
 */
void pa2ew_list_update_status_set( const int update_status )
{
	DL_NODE  *node    = NULL;
	_STAINFO *stainfo = NULL;

/* */
	DL_LIST_FOR_EACH_DATA( (DL_NODE *)SList->entry, node, stainfo ) {
		stainfo->update = update_status;
	}

	return;
}

/**
 * @brief
 *
 */
void pa2ew_list_obsolete_clear( void )
{
/* */
	dl_list_filter( (DL_NODE **)&SList->entry, obsolete_clear_cond, NULL, free_stainfo_and_chainfo );

	return;
}

/**
 * @brief
 *
 */
void pa2ew_list_tree_activate( void )
{
	void *_root = SList->root;

	SList->root      = SList->root_t;
	SList->root_t    = NULL;
	SList->timestamp = pa2ew_timenow_get();

	if ( _root ) {
		sleep_ew(1000);
		tdestroy(_root, dummy_func);
	}

	return;
}

/**
 * @brief
 *
 */
void pa2ew_list_tree_abandon( void )
{
	if ( SList->root_t )
		tdestroy(SList->root_t, dummy_func);

	SList->root_t = NULL;

	return;
}

/**
 * @brief
 *
 * @return int
 */
int pa2ew_list_total_station_get( void )
{
	DL_NODE  *node   = NULL;
	int       result = 0;

/* */
	DL_LIST_FOR_EACH( (DL_NODE *)SList->entry, node ) {
		result++;
	}
/* */
	SList->count = result;

	return result;
}

/**
 * @brief
 *
 * @return double
 */
double pa2ew_list_timestamp_get( void )
{
	return SList->timestamp;
}

/**
 * @brief
 *
 * @param action
 * @param arg
 */
void pa2ew_list_walk( void (*action)( void *, const int, void * ), void *arg )
{
	DL_NODE *node = NULL;
	int      i    = 0;

/* */
	DL_LIST_FOR_EACH( (DL_NODE *)SList->entry, node ) {
		action( DL_NODE_GET_DATA( node ), i++, arg );
	}

	return;
}

#if defined( _USE_SQL )
/**
 * @brief
 *
 * @param table_sta
 * @param table_chan
 * @param dbinfo
 * @param update
 * @return int
 */
static int fetch_list_sql( const char *table_sta, const char *table_chan, const DBINFO *dbinfo, const int update )
{
	int   result = 0;
	int   serial;
	int   nchannel;
	char  sta[TRACE2_STA_LEN] = { 0 };
	char  net[TRACE2_NET_LEN] = { 0 };
	char  loc[TRACE2_LOC_LEN] = { 0 };
	char *chan[PA2EW_MAX_CHAN_PER_STA] = { NULL };

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
	for ( int i = 0; i < PA2EW_MAX_CHAN_PER_STA; i++ )
		chan[i] = calloc(1, TRACE2_CHAN_LEN);

/* Start the SQL server connection for channel */
	stalist_start_persistent_sql( dbinfo );
/* Read station list from query result */
	while ( (sql_row = stalist_fetch_row_sql( sql_res )) != NULL ) {
	/* */
		extract_stainfo_mysql( &serial, sta, net, loc, sql_row, stalist_fetch_lengths_sql( sql_res ) );
	/* */
		nchannel = 0;
		if ( table_chan != NULL && strlen(table_chan) ) {
			nchannel = extract_chainfo_mysql(
				chan, stalist_chan_query_sql( dbinfo, table_chan, sta, net, loc, 1, COL_CHAN_CHANNEL )
			);
		}
	/* */
		if (
			append_stainfo_list(
				SList,
				create_new_stainfo( serial, sta, net, loc, nchannel, (const char **)chan ),
				update
			) != NULL
		) {
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
	stalist_end_thread_sql();

	if ( result > 0 )
		logit("o", "palert2ew: Read %d stations information from MySQL server success!\n", result);
	else
		logit("e", "palert2ew: Some errors happened when fetching station information from MySQL server!\n");
/* */
	for ( int i = 0; i < PA2EW_MAX_CHAN_PER_STA; i++ )
		free(chan[i]);

	return result;
}

/**
 * @brief
 *
 * @param serial
 * @param sta
 * @param net
 * @param loc
 * @param sql_row
 * @param row_lengths
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

/**
 * @brief
 *
 * @param chan
 * @param sql_res
 * @return int
 */
static int extract_chainfo_mysql( char *chan[], MYSQL_RES *sql_res )
{
/* */
	int i = 0;
	int result = 0;
	MYSQL_ROW sql_row;
	unsigned long *row_lengths;

/* */
	if ( sql_res ) {
	/* */
		i = 0;
		if ( (result = stalist_num_rows_sql( sql_res )) > 0 ) {
			while ( (sql_row = stalist_fetch_row_sql( sql_res )) ) {
				row_lengths = stalist_fetch_lengths_sql( sql_res );
				stalist_field_extract_sql( chan[i++], TRACE2_CHAN_LEN, sql_row[0], row_lengths[0] );
			/* */
				if ( i >= PA2EW_MAX_CHAN_PER_STA ) {
					i = PA2EW_MAX_CHAN_PER_STA;
					break;
				}
			}
		}
		stalist_free_result_sql( sql_res );
	}

	return result;
}
#else
/**
 * @brief Fake function
 *
 * @param table_sta
 * @param table_chan
 * @param dbinfo
 * @param update
 * @return int
 */
static int fetch_list_sql( const char *table_sta, const char *table_chan, const DBINFO *dbinfo, const int update )
{
	printf(
		"palert2ew: Skip the process of fetching station list from remote database "
		"'cause you did not define the _USE_SQL tag when compiling.\n"
	);
	return 0;
}
#endif

/**
 * @brief
 *
 * @return StaList*
 */
static StaList *init_sta_list( void )
{
	StaList *result = (StaList *)calloc(1, sizeof(StaList));

	if ( result ) {
		result->count     = 0;
		result->timestamp = pa2ew_timenow_get();
		result->entry     = NULL;
		result->root      = NULL;
		result->root_t    = NULL;
	}

	return result;
}

/**
 * @brief
 *
 * @param list
 */
static void destroy_sta_list( StaList *list )
{
	if ( list != (StaList *)NULL ) {
	/* */
		tdestroy(list->root, dummy_func);
		dl_list_destroy( (DL_NODE **)&list->entry, free_stainfo_and_chainfo );
		free(list);
	}

	return;
}

/**
 * @brief Appending the new client to the client list.
 *
 * @param list
 * @param stainfo
 * @param update
 * @return _STAINFO*
 */
static _STAINFO *append_stainfo_list( StaList *list, _STAINFO *stainfo, const int update )
{
	_STAINFO *result = NULL;
	void    **_root  = update == PA2EW_LIST_UPDATING ? &list->root : &list->root_t;

/* */
	if ( list && stainfo ) {
		if ( (result = tfind(stainfo, _root, compare_serial)) == NULL ) {
		/* Insert the station information into binary tree */
			if ( dl_node_append( (DL_NODE **)&list->entry, stainfo ) == NULL ) {
				logit("e", "palert2ew: Error insert station into linked list!\n");
				goto except;
			}
			if ( (result = tsearch(stainfo, &list->root_t, compare_serial)) == NULL ) {
				logit("e", "palert2ew: Error insert station into binary tree!\n");
				goto except;
			}
		}
		else if ( update == PA2EW_LIST_UPDATING ) {
			update_stainfo_and_chainfo( *(_STAINFO **)result, stainfo );
			if ( (result = tsearch(stainfo, &list->root_t, compare_serial)) == NULL ) {
				logit("e", "palert2ew: Error insert station into binary tree!\n");
				goto except;
			}
			if ( (*(_STAINFO **)result)->chaptr != stainfo->chaptr )
				free_stainfo_and_chainfo(stainfo);
			else
				free(stainfo);
		}
		else {
			logit("o", "palert2ew: Serial(%d) is already in the list, skip it!\n", stainfo->serial);
			free_stainfo_and_chainfo( stainfo );
		}
	}

	return result ? *(_STAINFO **)result : NULL;
/* Exception handle */
except:
	free_stainfo_and_chainfo( stainfo );
	return NULL;
}

/**
 * @brief Creating new station info memory space with the input value.
 *
 * @param serial
 * @param sta
 * @param net
 * @param loc
 * @param nchannel
 * @param chan
 * @return _STAINFO*
 */
static _STAINFO *create_new_stainfo(
	const int serial, const char *sta, const char *net,
	const char *loc, const int nchannel, const char *chan[]
) {
	_STAINFO *result = (_STAINFO *)calloc(1, sizeof(_STAINFO));

/* */
	if ( result ) {
		enrich_stainfo_raw( result, serial, sta, net, loc );
	/* */
		if ( nchannel )
			enrich_chainfo_raw( result, nchannel, chan );
		else
			enrich_chainfo_default( result );
	/* */
		if ( !result->chaptr ) {
			logit(
				"e", "palert2ew: Error created the channel memory for station %s.%s.%s!\n",
				result->sta, result->net, result->loc
			);
			free(result);
			result = NULL;
		}
	}

	return result;
}

/**
 * @brief
 *
 * @param stainfo
 * @return _CHAINFO*
 */
static _CHAINFO *enrich_chainfo_default( _STAINFO *stainfo )
{
#define X(a, b, c) b,
	const char *chan[] = {
		PALERT_M1_CHAN_TABLE
	};
#undef X

	return enrich_chainfo_raw( stainfo, PA2EW_DEF_CHAN_PER_STA, chan );
}

/**
 * @brief
 *
 * @param stainfo
 * @param serial
 * @param sta
 * @param net
 * @param loc
 * @return _STAINFO*
 */
static _STAINFO *enrich_stainfo_raw(
	_STAINFO *stainfo, const int serial, const char *sta, const char *net, const char *loc
) {
/* */
	stainfo->update = PA2EW_PALERT_INFO_UPDATED;
	stainfo->serial = serial;
	stainfo->chaptr = NULL;
	stainfo->buffer = NULL;
	strncpy(stainfo->sta, sta, TRACE2_STA_LEN);
	stainfo->sta[TRACE2_STA_LEN - 1] = '\0';
	strncpy(stainfo->net, net, TRACE2_NET_LEN);
	stainfo->net[TRACE2_NET_LEN - 1] = '\0';
	strncpy(stainfo->loc, loc, TRACE2_LOC_LEN);
	stainfo->loc[TRACE2_LOC_LEN - 1] = '\0';

	return stainfo;
}

/**
 * @brief
 *
 * @param stainfo
 * @param nchannel
 * @param chan
 * @return _CHAINFO*
 */
static _CHAINFO *enrich_chainfo_raw( _STAINFO *stainfo, const int nchannel, const char *chan[] )
{
	_CHAINFO *chainfo = (_CHAINFO *)calloc((uint16_t)nchannel, sizeof(_CHAINFO));

/* */
	stainfo->nchannel = (uint16_t)nchannel;
	stainfo->chaptr   = chainfo;

	if ( chainfo != NULL ) {
		for ( int i = 0; i < nchannel; i++ ) {
			chainfo[i].seq = i;
			strncpy(chainfo[i].chan, chan[i], TRACE2_CHAN_LEN);
			chainfo[i].chan[TRACE2_CHAN_LEN - 1] = '\0';
			chainfo[i].last_endtime = -1.0;
		}
	}

	return chainfo;
}

/**
 * @brief
 *
 * @param dest
 * @param src
 * @return _STAINFO*
 */
static _STAINFO *update_stainfo_and_chainfo( _STAINFO *dest, const _STAINFO *src )
{
/* */
	if ( strcmp(dest->sta, src->sta) )
		strcpy(dest->sta, src->sta);
	if ( strcmp(dest->net, src->net) )
		strcpy(dest->net, src->net);
	if ( strcmp(dest->loc, src->loc) )
		strcpy(dest->loc, src->loc);
/* */
	if ( dest->nchannel != src->nchannel ) {
		free(dest->chaptr);
		dest->chaptr = NULL;
	}
	else {
		for ( int i = 0; i < dest->nchannel; i++ ) {
			if ( strcmp(((_CHAINFO *)dest->chaptr)[i].chan, ((_CHAINFO *)src->chaptr)[i].chan) ) {
				free(dest->chaptr);
				dest->chaptr = NULL;
				break;
			}
		}
	}
/* */
	if ( !dest->chaptr )
		dest->chaptr = src->chaptr;
/* */
	dest->update = PA2EW_PALERT_INFO_UPDATED;

	return dest;
}

/**
 * @brief
 *
 * @param node
 * @param arg
 * @return int
 */
static int obsolete_clear_cond( void *node, void *arg )
{
	_STAINFO *stainfo = (_STAINFO *)node;

	if ( stainfo->update == PA2EW_PALERT_INFO_OBSOLETE )
		return 1;

	return 0;
}

/**
 * @brief
 *
 * @param node_a
 * @param node_b
 * @return int
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

/**
 * @brief
 *
 * @param node
 */
static void dummy_func( void *node )
{
	return;
}

/**
 * @brief
 *
 * @param node
 */
static void free_stainfo_and_chainfo( void *node )
{
	_STAINFO *stainfo = (_STAINFO *)node;

/* */
	if ( stainfo->buffer )
		free(stainfo->buffer);
/* */
	free(stainfo->chaptr);
	free(stainfo);

	return;
}
