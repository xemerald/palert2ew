/**
 * @file dbinfo.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Header file for database information struct.
 * @date 2020-08-01
 * @copyright Copyright (c) 2020
 *
 */

#pragma once

/**
 * @brief Define the character length of parameters
 *
 */
#define  MAX_HOST_LENGTH      256
#define  MAX_USER_LENGTH      16
#define  MAX_PASSWORD_LENGTH  32
#define  MAX_DATABASE_LENGTH  64
#define  MAX_TABLE_LEGTH      64

/**
 * @brief Database login information
 *
 */
typedef struct {
	char host[MAX_HOST_LENGTH];
	char user[MAX_USER_LENGTH];
	char password[MAX_PASSWORD_LENGTH];
	char database[MAX_DATABASE_LENGTH];
	long port;
} DBINFO;

#define DBINFO_INIT(_DB_INFO_) \
		((_DB_INFO_) = (DBINFO){ { 0 }, { 0 }, { 0 }, { 0 }, 0 })
