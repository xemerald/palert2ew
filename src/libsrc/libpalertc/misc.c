/**
 * @file misc.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief
 * @version 0.1
 * @date 2024-06-03
 *
 * @copyright Copyright (c) 2024
 *
 */

/* Standard C header include */
#include <stdint.h>
#include <stdio.h>
/* */
#include <misc.h>

/* */
static uint16_t cal_crc16_low( const uint8_t );
/* */
static uint16_t CRC16_Table[256] = { 0 };
static uint8_t  CRC16_Ready = 0;

/**
 * @brief Turn the broken time structure into calendar time(UTC)
 *
 * @param year
 * @param mon
 * @param day
 * @param hour
 * @param min
 * @param sec
 * @return unsigned long
 */
unsigned long misc_mktime( unsigned int year, unsigned int mon, unsigned int day, unsigned int hour, unsigned int min, unsigned int sec )
{
	if ( 0 >= (int)(mon -= 2) ) {
	/* Puts Feb. last since it has leap day */
		mon += 12;
		year--;
	}

	return ((((unsigned long)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day) + year * 365 - 719499) * 24 + hour) * 60 + min) * 60 + sec;
}

/**
 * @brief
 *
 * @param dest
 * @param first_num
 * @param second_num
 * @param third_num
 * @param fourth_num
 * @return char*
 */
char *misc_ipv4str_gen( char *dest, uint8_t first_num, uint8_t second_num, uint8_t third_num, uint8_t fourth_num )
{
	sprintf(dest, "%d.%d.%d.%d", first_num, second_num, third_num, fourth_num);

	return dest;
}


/**
 * @brief
 *
 */
void misc_crc16_init( void )
{
/* */
	for ( int i = 0x00; i <= 0xff; i++ )
		CRC16_Table[i & 0xff] = cal_crc16_low( i & 0xff );
/* */
	CRC16_Ready = 1;

	return;
}

/*
 * pa2ew_misc_crc8_cal() - A CRC-8 calculation function
 */

/**
 * @brief
 *
 * @param data
 * @param size
 * @return uint8_t
 */
uint16_t misc_crc16_cal( const void *data, const size_t size )
{
	const uint8_t *ptr = data;
	const uint8_t *end;
	uint16_t       result = PALERTC_MISC_CRC16_INIT;

/* */
	if ( !CRC16_Ready )
		misc_crc16_init();
/* */
	if ( ptr ) {
	/* */
		end = ptr + size;
		while ( ptr < end )
			result = CRC16_Table[result ^ *ptr++];
	}

	return result;
}


/**
 * @brief
 *
 */
static uint16_t cal_crc16_low( const uint8_t data )
{
	uint16_t result = PALERTC_MISC_CRC16_INIT;

/* */
	result ^= data;
	for ( int i = 0; i < 8; i++ ) {
		if ( result & 0x01 )
			result = (result >> 1) ^ PALERTC_MISC_CRC16_POLY;
		else
			result >>= 1;
	}

	return result;
}
