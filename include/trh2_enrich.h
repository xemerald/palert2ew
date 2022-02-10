/*
 * trh2_enrich.h
 *
 * Header file for enrich tracebuf 2 header.
 *
 * Benjamin Yang
 * Department of Geology
 * National Taiwan University
 *
 * February, 2022
 *
 */
#pragma once
/* For TRACE2_HEADER */
#include <trace_buf.h>

/* Function prototype */
TRACE2_HEADER *trh2_enrich(
	TRACE2_HEADER *, const char *, const char *, const char *, const int, const double, const double
);
