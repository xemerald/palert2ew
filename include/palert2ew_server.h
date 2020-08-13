#pragma once

int  PalertServerInit( const unsigned int );                      /* Initialize the independent Palert server */
int  ReadPalertsData( const unsigned short, const unsigned int ); /* Read the data from each Palert and put it into queue */
int  CheckPalertConn( void );                                     /* Check connections of all Palerts */
int  GetAcceptSocket( void );                                     /* Return the accept socket number */
void PalertServerEnd( void );                                     /* End process of Palert server */
