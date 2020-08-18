/*
 * palert2ew_msg_queue.h
 *
 * Header file for construct main messages queue.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * August, 2020
 *
 */
#pragma once

/* Header needed for PACKET, PREPACKET & _STAINFO */
#include <palert2ew.h>

/* Function prototype */
/* Initialization function of message queue and mutex */
int  pa2ew_msgqueue_init( const unsigned long );
void pa2ew_msgqueue_end( void );                    /* End process of message queue */
int  pa2ew_msgqueue_dequeue( PACKET *, size_t * );  /* Pop-out received message from main queue */
int  pa2ew_msgqueue_enqueue( PACKET *, size_t );    /* Put the compelete packet into the main queue. */
int  pa2ew_msgqueue_prequeue( _STAINFO *, const PREPACKET * );
