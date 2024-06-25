/**
 * @file libpalertc.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for Palert Communication C library.
 * @version 0.1
 * @date 2024-06-02
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
/* */
#include "samprate.h"
#include "trigmode.h"
#include "mode1.h"
#include "mode4.h"
#include "mode16.h"
/* */
#define LIBPALERTC_VERSION "2.0.0"    //!< Library version
#define LIBPALERTC_RELEASE "2024.06.05"  //!< Library release date
/* */
#define PALERT_PKT_MODE1  0x01
#define PALERT_PKT_MODE2  0x02
#define PALERT_PKT_MODE4  0x04
#define PALERT_PKT_MODE16 0x10
/* */
#define PALERT_SET_IP     0
#define PALERT_NTP_IP     1
#define PALERT_TCP0_IP    2
#define PALERT_TCP1_IP    3
#define PALERT_TCP2_IP    4
#define PALERT_ADMIN0_IP  5
#define PALERT_ADMIN1_IP  6
#define PALERT_ADMIN2_IP  7
#define PALERT_ALLOW0_IP  8
#define PALERT_ALLOW1_IP  9
#define PALERT_ALLOW2_IP  10

/* Export functions's prototypes, which are inside general.c */
int pac_mode_get( const void * );
int pac_sync_check( const void * );
int pac_ntp_sync_check( const void * );
int pac_pktlen_get( const void * );
int pac_serial_get( const void * );
int pac_cwb2020_int_trans( const int );
