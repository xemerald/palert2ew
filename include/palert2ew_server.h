/*
 * palert2ew_server.h
 *
 * Header file for setting up server connections waiting for Palerts.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * August, 2020
 *
 */
#pragma once
/* Network related header include */
#include <arpa/inet.h>
/* */
#include <palert2ew.h>
#define LISTENQ  128

/* Connection descriptors struct */
typedef struct {
	int       sock;
	int       port;
	char      ip[INET6_ADDRSTRLEN];
	uint8_t   sync_errors;
	uint16_t  packet_type;
	double    last_act;
	LABEL     label;
} CONNDESCRIP;

/* */
typedef struct {
	int                 epoll_fd;
	uint8_t            *buffer;
	struct epoll_event *evts;
} PALERT_THREAD_SET;

/* Macro */
#define RESET_CONNDESCRIP(CONN) \
	__extension__({ \
		memset((CONN), 0, sizeof(CONNDESCRIP)); \
		(CONN)->sock = -1; \
		(CONN)->label.serial = 0; \
	})


/* */
int  pa2ew_server_init( const int, const char * ); /* Initialize the independent Palert server */
void pa2ew_server_end( void );                                                   /* End process of Palert server */
void pa2ew_server_pconnect_walk( void (*)(const void *, const int, void *), void * );
int  pa2ew_server_proc( const int, const int );                                  /* Read the data from each Palert and put it into queue */
int  pa2ew_server_palerts_accept( const int );                                    /* Return the accept socket number */
int  pa2ew_server_pconnect_check( void );                                        /* Check connections of all Palerts */
CONNDESCRIP *pa2ew_server_pconnect_find( const uint16_t );
/* */
int          pa2ew_server_common_init( const int, const char *, const int, CONNDESCRIP **, int (*)( void ) );
CONNDESCRIP  pa2ew_server_common_accept( const int );
CONNDESCRIP *pa2ew_server_common_pconnect_find( const CONNDESCRIP *, const int, const uint16_t );
void pa2ew_server_common_pconnect_walk(
	const CONNDESCRIP *, const int, void (*)(const void *, const int, void *), void *
);
void pa2ew_server_common_pconnect_close( CONNDESCRIP *, const int );
