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

/* */
#define PALERTC_MISC_CRC16_INIT  0xFFFF
#define PALERTC_MISC_CRC16_POLY  0x8005

/* */
unsigned long misc_mktime( unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int );
char *misc_ipv4str_gen( char *, uint8_t, uint8_t, uint8_t, uint8_t );
