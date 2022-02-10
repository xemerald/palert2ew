/*
 *
 */
/* Standard C header include */
#include <stdio.h>
#include <string.h>
/* Earthworm environment header include */
#include <trace_buf.h>

/*
 * trh2_enrich() -
 */
TRACE2_HEADER *trh2_enrich(
	TRACE2_HEADER *dest, const char *sta, const char *net, const char *loc,
	const int nsamp, const double samprate, const double starttime
) {
/* */
	dest->pinno     = 0;
	dest->nsamp     = nsamp;
	dest->samprate  = samprate;
	dest->starttime = starttime;
	dest->endtime   = dest->starttime + (dest->nsamp - 1) / dest->samprate;
/* */
	strcpy(dest->sta, sta);
	strcpy(dest->net, net);
	strcpy(dest->loc, loc);
/* */
	dest->version[0] = TRACE2_VERSION0;
	dest->version[1] = TRACE2_VERSION1;

	strcpy(dest->quality, TRACE2_NO_QUALITY);
	strcpy(dest->pad    , TRACE2_NO_PAD    );

#if defined( _SPARC )
	strcpy(dest->datatype, "s4");   /* SUN IEEE integer */
#elif defined( _INTEL )
	strcpy(dest->datatype, "i4");   /* VAX/Intel IEEE integer */
#else
	fprintf(stderr, "palert2ew: warning _INTEL and _SPARC are both undefined.");
#endif

	return dest;
}
