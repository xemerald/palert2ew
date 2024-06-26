/**
 * @file palert2ew_client.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for setting up client connections to Palert server.
 * @date 2020-08-01
 *
 * @copyright Copyright (c) 2020
 *
 */

#pragma once

/**
 * @name Export functions' prototype
 *
 */
int  pa2ew_client_init( const char *, const char * );  /* Initialize the  */
void pa2ew_client_end( void );                         /* End process of Palert client */
int  pa2ew_client_stream( void );                      /* Read the data from Palert server and put it into queue */
