#pragma once

/* Header needed for MSG_LOGO & SHM_INFO */
#include <earthworm.h>
#include <transport.h>

/* Header needed for PACKET, PREPACKET & _STAINFO */
#include <palert2ew.h>

/* Function prototype */
int MsgQueueInit( const unsigned long, SHM_INFO *, MSG_LOGO * );     /* Initialization function of message queue and mutex */
int MsgEnqueue( PREPACKET *, _STAINFO * );       /* Stack received message into queue of station or main queue */
int MsgDequeue( PACKET *, long * );  /* Pop-out received message from main queue */

void MsgQueueEnd( void );                         /* End process of message queue */
