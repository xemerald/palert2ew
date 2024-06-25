/**
 * @file misc.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief
 * @version 0.1
 * @date 2024-06-02
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
/* */
#include <stdint.h>
#include <time.h>

/* */
#define PALERTC_MISC_CRC16_INIT  0xFFFF
#define PALERTC_MISC_CRC16_POLY  0xA001

/* */
time_t   misc_mktime( int, int, int, int, int, int );
char    *misc_ipv4str_gen( char *, uint8_t, uint8_t, uint8_t, uint8_t );
void     misc_crc16_init( void );
uint16_t misc_crc16_cal( const void *, const size_t );
