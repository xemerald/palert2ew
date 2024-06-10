/**
 * @file samprate.h
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for Palert data packet.
 * @version 0.1
 * @date 2024-06-02
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

/*
 * Palert default sampling rate information
 */
#define PALERT_SAMPRATE_TABLE \
		X(PALERT_SAMPRATE_20HZ  ,   20.0,    0.05) \
		X(PALERT_SAMPRATE_50HZ  ,   50.0,    0.02) \
		X(PALERT_SAMPRATE_100HZ ,  100.0,    0.01) \
		X(PALERT_SAMPRATE_200HZ ,  200.0,   0.005) \
		X(PALERT_SAMPRATE_500HZ ,  500.0,   0.002) \
		X(PALERT_SAMPRATE_1000HZ, 1000.0,   0.001) \
		X(PALERT_SAMPRATE_2000HZ, 2000.0,  0.0005) \
		X(PALERT_SAMPRATE_COUNT ,    0.0,     0.0) \

#define X(a, b, c) a,
typedef enum {
	PALERT_SAMPRATE_TABLE
} PALERT_SAMPRATES;
#undef X

#define PALERT_DEFAULT_SAMPRATE  100
#define PALERT_MAX_SAMPRATE      2000
