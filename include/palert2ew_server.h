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
int  pa2ew_server_init( const unsigned int );            /* Initialize the independent Palert server */
void pa2ew_server_end( void );                           /* End process of Palert server */
int  pa2ew_server_stream( const int, const int );        /* Read the data from each Palert and put it into queue */
int  pa2ew_server_conn_check( void );                    /* Check connections of all Palerts */
int  pa2ew_server_palert_accept( const int );            /* Return the accept socket number */
