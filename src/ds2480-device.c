#include "ibutton/ds2480.h"
#include "ds2480-device.h"
#include "transport.h"

int ds2480_dev_detect_handle(struct ds2480_device *dev, struct transport *t)
{
	unsigned char buf[8];

	/* We expect a 0xC1 timing byte. */
	if (transport_read_all(t, buf, 1) == -1)
		return -1;

	if (buf[0] != 0xC1)
		return -1;

	if (transport_read_all(t, buf, 5) == -1)
		return -1;

	/* default PDSRC = 1.37Vus */
	if (buf[0] != (CMD_CONFIG | PARMSEL_SLEW | PARMSET_Slew1p37Vus))
		return -1;

	/* default W1LT = 10us */
	if (buf[1] != (CMD_CONFIG | PARMSEL_WRITE1LOW | PARMSET_Write10us))
		return -1;

	/* default DSO/WORT = 8us */
	if (buf[2] != (CMD_CONFIG | PARMSEL_SAMPLEOFFSET | PARMSET_SampOff8us))
		return -1;

	/* command to read the baud rate (to test command block) */
	if (buf[3] != (CMD_CONFIG | PARMSEL_PARMREAD | (PARMSEL_BAUDRATE >> 3)))
		return -1;

	/* 1 bit operation (to test 1-Wire block) */
	if (buf[4] != (CMD_COMM | FUNCTSEL_BIT | PARMSET_9600 | BITPOL_ONE))
		return -1;

	/* XXX: figure out what these fields mean.  For now we just send the
	 * expected answer.
	 */
	buf[0] = buf[1] = buf[2] = 0;
	buf[3] = PARMSET_9600;
	buf[4] = 0x90 | PARMSET_9600;
	transport_write(t, buf, 5);

	return 0;
}
