#pragma once

int  PalertClientInit( const char *, const char * );  /* Initialize the  */
int  ReadServerData( void );                          /* Read the data from Palert server and put it into queue */
void PalertClientEnd( void );                         /* End process of Palert client */
