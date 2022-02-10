/*
 * palert2ew_server.h
 *
 * Header file for setting up extension server connections waiting for Palerts.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * February, 2022
 *
 */
#pragma once
/* */
#include <earthworm.h>
/* */
#include <palert2ew.h>
/* */
int  pa2ew_server_ext_init( const char *, const MSG_LOGO ); /* Initialize the independent Palert server */
void pa2ew_server_ext_end( void );                                                   /* End process of Palert server */
int  pa2ew_server_ext_proc( const int, const int );                                  /* Read the data from each Palert and put it into queue */
int  pa2ew_server_ext_req_send( const _STAINFO *, const char *, const int );         /* */
int  pa2ew_server_ext_conn_check( void );                                            /* Check connections of all Palerts */
int  pa2ew_server_ext_conn_reset( const _STAINFO * );
