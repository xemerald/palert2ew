/*
 * stalist.c
 *
 * Tool for fetching station information from remote database.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * March, 2020
 *
 */
/* Standard C header include */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* Local header include */
#include <stalist.h>

/* Internal functions' prototype */
static MYSQL_RES *query_sql( const DBINFO *, const char *, const size_t );
static char *gen_select_str( char *, const char *, GET_COLUMN_NAME, int, va_list );
static char *get_sta_column_name( const STALIST_COL_LIST );
static char *get_chan_column_name( const STALIST_COL_LIST );

/*
 *
 */
static MYSQL *SQL = NULL;

/*
 * stalist_sta_query_sql() -
 */
MYSQL_RES *stalist_sta_query_sql( const DBINFO *dbinfo, const char *table, const int num_col, ... )
{
	char       query[4096];
	size_t     query_len;
	va_list    ap;

	va_start(ap, num_col);
	query_len = strlen(gen_select_str( query, table, get_sta_column_name, num_col, ap ));
	va_end(ap);

	return query_sql( dbinfo, query, query_len );
}

/*
 * stalist_chan_query_sql() -
 */
MYSQL_RES *stalist_chan_query_sql(
	const DBINFO *dbinfo, const char *table, const char *sta, const char *net, const char *loc, const int num_col, ...
) {
	char       query[4096];
	char       tmpquery[512];
	size_t     query_len;
	va_list    ap;

/* */
	va_start(ap, num_col);
	gen_select_str( query, table, get_chan_column_name, num_col, ap );
	va_end(ap);
/* */
	sprintf(
		tmpquery, " WHERE %s='%s' && %s='%s' && %s='%s'",
		get_sta_column_name( (COL_STA_LIST)COL_STA_STATION ), sta,
		get_sta_column_name( (COL_STA_LIST)COL_STA_NETWORK ), net,
		get_sta_column_name( (COL_STA_LIST)COL_STA_LOCATION ), loc
	);
	query_len = strlen(strcat(query, tmpquery));

	return query_sql( dbinfo, query, query_len );
}

/*
 * stalist_fetch_row_sql() -
 */
MYSQL_ROW stalist_fetch_row_sql( MYSQL_RES *res )
{
	return mysql_fetch_row(res);
}

/*
 * stalist_num_rows_sql() -
 */
int stalist_num_rows_sql( MYSQL_RES *res )
{
	return mysql_num_rows(res);
}

/*
 * stalist_free_result_sql() -
 */
void stalist_free_result_sql( MYSQL_RES *res )
{
/* */
	mysql_free_result(res);
	return;
}

/*
 * stalist_free_result_sql() -
 */
MYSQL *stalist_start_persistent_sql( const DBINFO *dbinfo )
{
/* */
	if ( SQL == NULL ) {
	/* Connect to database */
		SQL = mysql_init(NULL);
		mysql_options(SQL, MYSQL_SET_CHARSET_NAME, "utf8");
		mysql_real_connect(SQL, dbinfo->host, dbinfo->user, dbinfo->password, dbinfo->database, dbinfo->port, NULL, 0);
	}

	return SQL;
}

/*
 * stalist_free_result_sql() -
 */
void stalist_close_persistent_sql( void )
{
/* */
	if ( SQL != NULL ) {
		mysql_close(SQL);
		SQL = NULL;
	}
	return;
}

/*
 * query_sql() - Get stations list from MySQL server
 */
static MYSQL_RES *query_sql( const DBINFO *dbinfo, const char *query, const size_t query_len )
{
	MYSQL_RES *result = NULL;

/* */
	if ( SQL == NULL ) {
		MYSQL *sql = mysql_init(NULL);
	/* Connect to database */
		mysql_options(sql, MYSQL_SET_CHARSET_NAME, "utf8");
		if ( mysql_real_connect(sql, dbinfo->host, dbinfo->user, dbinfo->password, dbinfo->database, dbinfo->port, NULL, 0) != NULL )
			if ( !mysql_real_query(sql, query, query_len) )
				result = mysql_store_result(sql);
		mysql_close(sql);
	}
	else {
		if ( !mysql_real_query(SQL, query, query_len) )
			result = mysql_store_result(SQL);
	}

	return result;
}

/*
 * gen_select_str() -
 */
static char *gen_select_str(
	char *buffer, const char *table, GET_COLUMN_NAME get_column_name, int num_col, va_list ap
) {
/* */
	buffer[0] = '\0';
/* */
	sprintf(buffer, "SELECT ");
	for ( ; num_col > 0; num_col-- ) {
		if ( get_column_name == get_sta_column_name ) {
			COL_STA_LIST col = va_arg(ap, COL_STA_LIST);
			strcat(buffer, get_column_name( col ));
		}
		else if ( get_column_name == get_chan_column_name ) {
			COL_CHAN_LIST col = va_arg(ap, COL_CHAN_LIST);
			strcat(buffer, get_column_name( col ));
		}
	 	strcat(buffer, num_col > 1 ? "," : " ");
	}
/* */
	strcat(buffer, "FROM ");
	strcat(buffer, table);

	return buffer;
}

/*
 * get_sta_column_name() -
 */
static char *get_sta_column_name( const STALIST_COL_LIST col )
{
#define X(a, b) b,
	static char *col_name[] = {
		COL_STA_LIST_TABLE
	};
#undef X

	return col_name[col.col_sta];
}

/*
 * get_chan_column_name() -
 */
static char *get_chan_column_name( const STALIST_COL_LIST col )
{
#define X(a, b) b,
	static char *col_name[] = {
		COL_CHAN_LIST_TABLE
	};
#undef X

	return col_name[col.col_chan];
}
