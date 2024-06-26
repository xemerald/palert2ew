/**
 * @file palert2ew_server.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for setting up server connections waiting for Palerts.
 * @date 2020-08-01
 *
 * @copyright Copyright (c) 2020
 *
 */

#pragma once

/**
 * @name Network related header include
 *
 */
#include <arpa/inet.h>

/**
 * @name Local header include
 *
 */
#include <palert2ew.h>

/**
 * @brief
 *
 */
#define LISTENQ  128

/**
 * @brief Connection descriptors struct
 *
 */
typedef struct {
	int      sock;
	int      port;
	char     ip[INET6_ADDRSTRLEN];
	uint8_t  sync_errors;
	double   last_act;
	LABEL    label;
} CONNDESCRIP;

/**
 * @brief
 *
 */
typedef struct {
	int                 epoll_fd;
	uint8_t            *buffer;
	struct epoll_event *evts;
} PALERT_THREAD_SET;

/**
 * @brief
 *
 */
#define RESET_CONNDESCRIP(CONN) \
		__extension__({ \
			memset((CONN), 0, sizeof(CONNDESCRIP)); \
			(CONN)->sock = -1; \
			(CONN)->label.staptr = NULL; \
			(CONN)->label.packmode = 0; \
		})

/**
 * @name Export functions' prototype
 *
 */
int  pa2ew_server_init( const int, const char * ); /* Initialize the independent Palert server */
void pa2ew_server_end( void );                                                   /* End process of Palert server */
void pa2ew_server_pconnect_walk( void (*)(const void *, const int, void *), void * );
int  pa2ew_server_proc( const int, const int );                                  /* Read the data from each Palert and put it into queue */
int  pa2ew_server_palerts_accept( const int );                                    /* Return the accept socket number */
int  pa2ew_server_pconnect_check( void );                                        /* Check connections of all Palerts */
CONNDESCRIP *pa2ew_server_pconnect_find( const uint16_t );
int          pa2ew_server_common_init( const int, const char *, const int, CONNDESCRIP **, int (*)( void ) );
CONNDESCRIP  pa2ew_server_common_accept( const int );
CONNDESCRIP *pa2ew_server_common_pconnect_find( const CONNDESCRIP *, const int, const uint16_t );
void pa2ew_server_common_pconnect_walk(
	const CONNDESCRIP *, const int, void (*)(const void *, const int, void *), void *
);
void pa2ew_server_common_pconnect_close( CONNDESCRIP *, const int );
