#ifndef INCLUDE_RTL_433_DEVICES_H_
#define INCLUDE_RTL_433_DEVICES_H_

#include "bitbuffer.h"

#define DEVICES \
		DECL(fineoffset_wh1080) \
		DECL(fineoffset_XC0400)

typedef struct {
	char name[256];
	unsigned int modulation;
	float short_limit;
	float long_limit;
	float reset_limit;
	float gap_limit;
    float sync_width;
    float tolerance;
	int (*json_callback)(bitbuffer_t *bitbuffer);
	unsigned int disabled;
	unsigned demod_arg;	// Decoder specific optional argument
	char **fields;			// List of fields this decoder produces; required for CSV output. NULL-terminated.
} r_device;

#define DECL(name) extern r_device name;
DEVICES
#undef DECL

#endif /* INCLUDE_RTL_433_DEVICES_H_ */
