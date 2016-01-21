#include <stdio.h>
#include "ds1963s-error.h"

__thread int ds1963s_errno;

static char *__errors[] = {
	"Success",
	"Failed to open serial port",
	"DS2480B adapter not found",
	"Invalid DS1963S address",
	"Failed to set serial control bits",
	"Failed to change 1-Wire level",
	"DS1963S ibutton not found",
};

void ds1963s_perror(const char *s)
{
	size_t errnum = sizeof(__errors) / sizeof(char *);

	fprintf(stderr, "%s: ", s);

	if (ds1963s_errno < 0 || ds1963s_errno >= errnum)
		fprintf(stderr, "Unknown error number.\n");
	else
		fprintf(stderr, "%s.\n", __errors[ds1963s_errno]);
}
