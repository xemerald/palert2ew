/* Standard C header include */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* Local header include */
#include <stalist.h>

/* Internal functions' prototype */
static char *gen_select_str( char *, const char *, int, va_list );
static char *get_column_name( const COL_STA_LIST col );

/* */
static MYSQL *SQLConn;

/*
 * stalist_query_sql() - Get stations list from MySQL server
 */
MYSQL_RES *stalist_query_sql( const DBINFO *dbinfo, const char *tablename, const int num_col, ... )
{
	char    query[4096];
	size_t  qlength;
	va_list ap;

/* Connect to database */
	SQLConn = mysql_init(NULL);
	mysql_options(SQLConn, MYSQL_SET_CHARSET_NAME, "utf8");

	if ( !mysql_real_connect(SQLConn, dbinfo->host, dbinfo->user, dbinfo->password, dbinfo->database, dbinfo->port, NULL, 0) )
		return NULL;

/* Send SQL query */
	va_start(ap, num_col);
	qlength = strlen(gen_select_str( query, tablename, num_col, ap ));
	va_end(ap);
	if ( qlength ) {
		if ( mysql_real_query(SQLConn, query, qlength) ) {
		/* Close connection */
			mysql_close(SQLConn);
			return NULL;
		}
	}

/* Read station list from query result */
	return mysql_store_result(SQLConn);
}

/*
 * stalist_fetch_row_sql() -
 */
MYSQL_ROW stalist_fetch_row_sql( MYSQL_RES *res )
{
	return mysql_fetch_row(res);
}

/*
 *
 */
void stalist_end_sql( MYSQL_RES *res )
{
/* */
	mysql_free_result(res);
/* Close connection */
	mysql_close(SQLConn);

	return;
}

/*
 *
 */
static char *gen_select_str( char *buffer, const char *tablename, int num_col, va_list ap )
{
	COL_STA_LIST cslist = COL_STA_LIST_COUNT;

/* */
	buffer[0] = '\0';
	if ( (unsigned)num_col >= cslist ) return buffer;
/* */
	sprintf(buffer, "SELECT ");
	for ( ; num_col > 0; num_col-- ) {
		cslist = va_arg(ap, COL_STA_LIST);
		if ( cslist >= 0 && cslist < COL_STA_LIST_COUNT )
			strcat(buffer, get_column_name( cslist ));
	/* */
	 	strcat(buffer, num_col > 1 ? "," : " ");
	}
/* */
	strcat(buffer, "FROM ");
	strcat(buffer, tablename);
	strcat(buffer, ";");

	return buffer;
}

/*
 *
 */
static char *get_column_name( const COL_STA_LIST col )
{
#define X(a, b) b,
	static char *col_name[] = {
		COL_STA_LIST_TABLE
	}
#undef X

	return col_name[col];
}
