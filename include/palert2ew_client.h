/*
 *
 */
#pragma once

/* */
int  pa2ew_client_init( const char *, const char * );  /* Initialize the  */
void pa2ew_client_end( void );                         /* End process of Palert client */
int  pa2ew_client_stream( void );                      /* Read the data from Palert server and put it into queue */
