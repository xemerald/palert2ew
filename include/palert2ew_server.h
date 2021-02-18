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

/* */
int  pa2ew_server_init( const int, const char *, const char *, const MSG_LOGO ); /* Initialize the independent Palert server */
void pa2ew_server_end( void );                                                   /* End process of Palert server */
int  pa2ew_server_proc( const int, const int );                                  /* Read the data from each Palert and put it into queue */
int  pa2ew_server_ext_req_send( const _STAINFO *, const char *, const int );     /* */
int  pa2ew_server_conn_check( void );                                            /* Check connections of all Palerts */
int  pa2ew_server_palert_accept( const int );                                    /* Return the accept socket number */
