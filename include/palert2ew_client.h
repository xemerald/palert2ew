/*
 * palert2ew_client.h
 *
 * Header file for setting up client connections to Palert server.
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
int  pa2ew_client_init( const char *, const char * );  /* Initialize the  */
void pa2ew_client_end( void );                         /* End process of Palert client */
int  pa2ew_client_stream( void );                      /* Read the data from Palert server and put it into queue */
