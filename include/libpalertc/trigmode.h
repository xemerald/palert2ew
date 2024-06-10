/**
 * @file trigmode.h
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
 * Palert trigger mode information
 */
#define PALERT_TRIGMODE_VDIS_BIT     0x01
#define PALERT_TRIGMODE_PD_BIT       0x02
#define PALERT_TRIGMODE_PGA_BIT      0x04
#define PALERT_TRIGMODE_STA_LTA_BIT  0x08

#define PALERT_TRIGMODE_TABLE \
		X(PALERT_TRIGMODE_VDIS,    "vdisp",   0x01) \
		X(PALERT_TRIGMODE_PD,      "Pd",      0x02) \
		X(PALERT_TRIGMODE_PGA,     "PGA",     0x04) \
		X(PALERT_TRIGMODE_STA_LTA, "STA/LTA", 0x08) \
		X(PALERT_TRIGMODE_COUNT,   "NULL",    0xFF)

#define PALERT_TRIGMODE_STR_LENGTH  24

#define X(a, b, c) a,
typedef enum {
	PALERT_TRIGMODE_TABLE
} PALERT_TRIGMODES;
#undef X
