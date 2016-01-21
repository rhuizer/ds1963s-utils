#include "ds1963s-error.h"
#include "ibutton/ds2480.h"
#include "ibutton/shaib.h"

void owClearError(void);

int onewire_acquire(const char *port)
{
	int portnum;
   
	if ( (portnum = OpenCOMEx(port)) < 0) {
		ds1963s_errno = DS1963S_ERROR_OPENCOM;
		return -1;
	}
   
	if (!DS2480Detect(portnum)) {
		CloseCOM(portnum);
		ds1963s_errno = DS1963S_ERROR_NO_DS2480;
		return -1;
	}

	return portnum;
}

/* XXX: probably doesn't belong here. */
int onewire_ibutton_sha_find(int portnum, unsigned char *devAN)
{
	/* XXX: FindNewSHA can return FALSE without adding an error to the
	 * error stack (when it does not find any buttons).  We need to
	 * differentiate.
	 */
	owClearError();

	if (FindNewSHA(portnum, devAN, TRUE) == TRUE)
		return 0;

	/* XXX: this is dependent on FindNewSHA internals. */
	if (owHasErrors()) {
		ds1963s_errno = DS1963S_ERROR_SET_LEVEL;
		return -1;
	}

	/* We did not find any new SHA based ibuttons. */
	ds1963s_errno = DS1963S_ERROR_NOT_FOUND;
	return -1;
}
