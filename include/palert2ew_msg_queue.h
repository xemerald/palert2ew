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
/* */
#include <mem_circ_queue.h>
/* Header needed for PACKET, PREPACKET & _STAINFO */
#include <palert2ew.h>
/* */
#define PA2EW_GEN_MSG_LOGO_BY_SRC(PA2EW_MSG_SRC) \
		((MSG_LOGO){ (PA2EW_MSG_SRC), (PA2EW_MSG_SRC), (PA2EW_MSG_SRC) })

/* Function prototype */
int  pa2ew_msgqueue_init( const unsigned long, const unsigned long );  /* Initialization function of message queue and mutex */
void pa2ew_msgqueue_end( void );                                  /* End process of message queue */
int  pa2ew_msgqueue_dequeue( void *, size_t *, MSG_LOGO * );    /* Pop-out received message from main queue */
int  pa2ew_msgqueue_enqueue( void *, size_t, MSG_LOGO );        /* Put the compelete packet into the main queue. */
int  pa2ew_msgqueue_rawpacket( void *, size_t, int, MSG_LOGO );
