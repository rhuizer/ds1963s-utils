#include <assert.h>
#include "ibutton/ds2480.h"
#include "ds2480-device.h"
#include "transport.h"

void ds2480_dev_init(struct ds2480_device *dev)
{
	dev->mode                    = DS2480_MODE_COMMAND;
	dev->config.slew             = DS2480_PARAM_SLEW_VALUE_15Vus;
	dev->config.pulse12v         = DS2480_PARAM_PULSE12V_VALUE_512us;
	dev->config.pulse5v          = DS2480_PARAM_PULSE5V_VALUE_524ms;
	dev->config.write1low        = DS2480_PARAM_WRITE1LOW_VALUE_8us;
	dev->config.sampleoffset     = DS2480_PARAM_SAMPLEOFFSET_VALUE_8us;
	dev->config.activepulluptime = DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_3p0us;
	dev->config.baudrate         = DS2480_PARAM_BAUDRATE_VALUE_9600;
}

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

void ds2480_dev_reset(struct ds2480_device *dev, int speed)
{
	assert(dev != NULL);
	assert(speed >= 0 && speed < 3);
	assert(dev->mode == DS2480_MODE_COMMAND ||
	       dev->mode == DS2480_MODE_CHECK);

	dev->mode  = DS2480_MODE_COMMAND;
	dev->speed = speed;
}

static inline int
__ds2480_speed_parse(int speed)
{
	assert(speed >= 0 && speed <= 3);

	switch(speed) {
	case 0:
		return DS2480_SPEED_REGULAR;
	case 1:
		return DS2480_SPEED_FLEX;
	case 2:
		return DS2480_SPEED_OVERDRIVE;
	case 3:
		return DS2480_SPEED_REGULAR;
	}

	abort();
}

#if 0
int ds2480_dev_config_read(struct ds2480_device *dev, ...)
{
}
#endif

int ds2480_dev_config_read(struct ds2480_device *dev, int param)
{
	assert(dev != NULL);

	switch (param) {
	case DS2480_PARAM_SLEW:
		return dev->config.slew;
	case DS2480_PARAM_12VPULSE:
		return dev->config.pulse12v;
	case DS2480_PARAM_5VPULSE:
		return dev->config.pulse5v;
	case DS2480_PARAM_WRITE1LOW:
		return dev->config.write1low;
	case DS2480_PARAM_SAMPLEOFFSET:
		return dev->config.sampleoffset;
	case DS2480_PARAM_ACTIVEPULLUPTIME:
		return dev->config.activepulluptime;
	case DS2480_PARAM_BAUDRATE:
		return dev->config.baudrate;
	}

	return -1;
}

int ds2480_dev_config_write(struct ds2480_device *dev, int param, int value)
{
	assert(dev != NULL);

	switch (param) {
	case DS2480_PARAM_SLEW:
		if (value < 0 || value > 7)
			return -1;

		dev->config.slew = value;
		break;
	case DS2480_PARAM_12VPULSE:
		if (value < 0 || value > 7)
			return -1;

		dev->config.pulse12v = value;
		break;
	case DS2480_PARAM_5VPULSE:
		if (value < 0 || value > 7)
			return -1;

		dev->config.pulse5v = value;
		break;
	case DS2480_PARAM_WRITE1LOW:
		if (value < 0 || value > 7)
			return -1;

		dev->config.write1low = value;
		break;
	case DS2480_PARAM_SAMPLEOFFSET:
		if (value < 0 || value > 7)
			return -1;

		dev->config.sampleoffset = value;
		break;
	case DS2480_PARAM_ACTIVEPULLUPTIME:
		if (value < 0 || value > 7)
			return -1;

		dev->config.activepulluptime = value;
		break;
	case DS2480_PARAM_BAUDRATE:
		if (value < 0 || value > 3)
			return -1;

		dev->config.baudrate = value;
		break;
	default:
		return -1;
	}

	return 0;
}

static int
ds2480_dev_command_mode(struct ds2480_device *dev, struct transport *t)
{
	unsigned char buf[8];
	int speed;

	assert(dev != NULL);
	assert(t != NULL);
	assert(dev->mode == DS2480_COMMAND_MODE);

	if (transport_read_all(t, buf, 1) == -1)
		return -1;

	/* All command codes have their least significant bit set. */
	if ( (buf[0] & 1) == 0)
		return -1;

	/* Configuration commands have their most significant bit set to 0. */
	if (buf[0] & 0x80) {
		return 0;
	}

	/* Otherwise we use the top 3-bits to specify the operation. */
	switch (buf[0] >> 5) {
	case DS2480_COMMAND_SINGLE_BIT:
	case DS2480_COMMAND_SEARCH_ACCELERATOR_CONTROL:
	case DS2480_COMMAND_RESET:
		speed = __ds2480_speed_parse( (buf[0] >> 2) & 3);
		ds2480_dev_reset(dev, speed);
		if (transport_write_all(t, "\xcd", 1) == -1)
			return -1;
		break;
	case DS2480_COMMAND_PULSE:
	default:
		return -1;
	}

	return 0;
}

int ds2480_dev_run(struct ds2480_device *dev, struct transport *t)
{
	unsigned char buf[8];

	assert(dev != NULL);

	do {
		if (dev->mode == DS2480_MODE_COMMAND)
			ds2480_dev_command_mode(dev, t);
	} while (1);

	if (transport_read_all(t, buf, 1) == -1)
		return -1;

}
