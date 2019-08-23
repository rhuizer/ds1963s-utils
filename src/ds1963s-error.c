#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "ds1963s-error.h"

static char *__errors[] = {
	"Success",
	"Failed to open serial port",
	"DS2480B adapter not found",
	"Invalid address.  Valid addresses are 0..704",
	"Failed to set serial control bits",
	"Failed to change 1-Wire level",
	"DS1963S ibutton not found",
	"Invalid secret number",
	"Invalid secret length",
	"Invalid data length",
	"Access to device failed",
	"Failed to transmit data block",
	"Failed to erase scratchpad",
	"Failed to read scratchpad",
	"Data integrity error",
	"Failed to copy scratchpad",
	"Invalid page number.  Valid numbers are 0..21",
	"SHA function failed",
	"Copy Scratchpad failed",
	"Copy secret failed",
	"Read Scratchpad failed",
	"Match Scratchpad failed"
};

static size_t errnum = sizeof(__errors) / sizeof(char *);

void
ds1963s_perror(int errno, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ds1963s_vperror(errno, fmt, ap);
	va_end(ap);
}

void
ds1963s_vperror(int errno, const char *fmt, va_list ap)
{
	char buf[1024];
	char *msg;

	if (fmt && vasprintf(&msg, fmt, ap) == -1) {
		if (vsnprintf(buf, sizeof buf, fmt, ap) < 0)
			return;

		msg = buf;
	}

	if (fmt)
		fprintf(stderr, "%s: ", msg);

	if (errno < 0 || errno >= errnum)
		fprintf(stderr, "Unknown error number.\n");
	else
		fprintf(stderr, "%s.\n", __errors[errno]);


	if (fmt && msg != buf)
		free(msg);
}
