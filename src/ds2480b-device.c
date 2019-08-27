/* ds2480b-device.c
 *
 * A software implementation of the DS2480B serial to 1-wire driver.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * Copyright (C) 2016-2019  Ronald Huizer <rhuizer@hexpedition.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <assert.h>
#include <string.h>
#include "debug.h"
#include "ibutton/ds2480.h"
#include "1-wire-bus.h"
#include "ds2480b-device.h"
#include "transport.h"

#ifdef DEBUG

#define NAMEDEF(x)	[x] = #x

static const char * __command_names[8] = {
	NAMEDEF(DS2480_COMMAND_SINGLE_BIT),
	NAMEDEF(DS2480_COMMAND_SEARCH_ACCELERATOR_CONTROL),
	NAMEDEF(DS2480_COMMAND_RESET),
	NAMEDEF(DS2480_COMMAND_PULSE)
};

static const char * __speed_names[4] = {
	NAMEDEF(DS2480_SPEED_REGULAR),
	NAMEDEF(DS2480_SPEED_FLEX),
	NAMEDEF(DS2480_SPEED_OVERDRIVE)
};

static const char * __param_code_names[8] = {
	NAMEDEF(DS2480_PARAM_PARMREAD),
	NAMEDEF(DS2480_PARAM_SLEW),
	NAMEDEF(DS2480_PARAM_12VPULSE),
	NAMEDEF(DS2480_PARAM_5VPULSE),
	NAMEDEF(DS2480_PARAM_WRITE1LOW),
	NAMEDEF(DS2480_PARAM_SAMPLEOFFSET),
	NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME),
	NAMEDEF(DS2480_PARAM_BAUDRATE)
};

static const char *__param_value_names[8][8] = {
	[DS2480_PARAM_SLEW] = {
		NAMEDEF(DS2480_PARAM_SLEW_VALUE_15Vus),
		NAMEDEF(DS2480_PARAM_SLEW_VALUE_2p2Vus),
		NAMEDEF(DS2480_PARAM_SLEW_VALUE_1p65Vus),
		NAMEDEF(DS2480_PARAM_SLEW_VALUE_1p37Vus),
		NAMEDEF(DS2480_PARAM_SLEW_VALUE_1p1Vus),
		NAMEDEF(DS2480_PARAM_SLEW_VALUE_0p83Vus),
		NAMEDEF(DS2480_PARAM_SLEW_VALUE_0p7Vus),
		NAMEDEF(DS2480_PARAM_SLEW_VALUE_0p55Vus)
	},
	[DS2480_PARAM_12VPULSE] = {
		NAMEDEF(DS2480_PARAM_PULSE12V_VALUE_32us),
		NAMEDEF(DS2480_PARAM_PULSE12V_VALUE_64us),
		NAMEDEF(DS2480_PARAM_PULSE12V_VALUE_128us),
		NAMEDEF(DS2480_PARAM_PULSE12V_VALUE_256us),
		NAMEDEF(DS2480_PARAM_PULSE12V_VALUE_512us),
		NAMEDEF(DS2480_PARAM_PULSE12V_VALUE_1024us),
		NAMEDEF(DS2480_PARAM_PULSE12V_VALUE_2048us),
		NAMEDEF(DS2480_PARAM_PULSE12V_VALUE_infinite)
	},
	[DS2480_PARAM_5VPULSE] = {
		NAMEDEF(DS2480_PARAM_PULSE5V_VALUE_16p4ms),
		NAMEDEF(DS2480_PARAM_PULSE5V_VALUE_65p5ms),
		NAMEDEF(DS2480_PARAM_PULSE5V_VALUE_131ms),
		NAMEDEF(DS2480_PARAM_PULSE5V_VALUE_262ms),
		NAMEDEF(DS2480_PARAM_PULSE5V_VALUE_524ms),
		NAMEDEF(DS2480_PARAM_PULSE5V_VALUE_1p05s),
		NAMEDEF(DS2480_PARAM_PULSE5V_VALUE_2p10s),
		NAMEDEF(DS2480_PARAM_PULSE5V_VALUE_infinite)
	},
	[DS2480_PARAM_WRITE1LOW] = {
		NAMEDEF(DS2480_PARAM_WRITE1LOW_VALUE_8us),
		NAMEDEF(DS2480_PARAM_WRITE1LOW_VALUE_9us),
		NAMEDEF(DS2480_PARAM_WRITE1LOW_VALUE_10us),
		NAMEDEF(DS2480_PARAM_WRITE1LOW_VALUE_11us),
		NAMEDEF(DS2480_PARAM_WRITE1LOW_VALUE_12us),
		NAMEDEF(DS2480_PARAM_WRITE1LOW_VALUE_13us),
		NAMEDEF(DS2480_PARAM_WRITE1LOW_VALUE_14us),
		NAMEDEF(DS2480_PARAM_WRITE1LOW_VALUE_15us)
	},
	[DS2480_PARAM_SAMPLEOFFSET] = {
		NAMEDEF(DS2480_PARAM_SAMPLEOFFSET_VALUE_3us),
		NAMEDEF(DS2480_PARAM_SAMPLEOFFSET_VALUE_4us),
		NAMEDEF(DS2480_PARAM_SAMPLEOFFSET_VALUE_5us),
		NAMEDEF(DS2480_PARAM_SAMPLEOFFSET_VALUE_6us),
		NAMEDEF(DS2480_PARAM_SAMPLEOFFSET_VALUE_7us),
		NAMEDEF(DS2480_PARAM_SAMPLEOFFSET_VALUE_8us),
		NAMEDEF(DS2480_PARAM_SAMPLEOFFSET_VALUE_9us),
		NAMEDEF(DS2480_PARAM_SAMPLEOFFSET_VALUE_10us)
	},
	[DS2480_PARAM_ACTIVEPULLUPTIME] = {
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_0p0us),
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_0p0us),
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_0p5us),
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_1p0us),
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_1p5us),
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_2p0us),
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_2p5us),
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_3p0us),
		NAMEDEF(DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_3p5us)
	},
	[DS2480_PARAM_BAUDRATE] = {
		NAMEDEF(DS2480_PARAM_BAUDRATE_VALUE_9600),
		NAMEDEF(DS2480_PARAM_BAUDRATE_VALUE_19200),
		NAMEDEF(DS2480_PARAM_BAUDRATE_VALUE_57600),
		NAMEDEF(DS2480_PARAM_BAUDRATE_VALUE_115200)
	}
};
#endif

void ds2480b_dev_init(struct ds2480b_device *dev)
{
	assert(dev != NULL);

	dev->accelerator             = 0;
	dev->mode                    = DS2480_MODE_INACTIVE;
	dev->config.slew             = DS2480_PARAM_SLEW_VALUE_15Vus;
	dev->config.pulse12v         = DS2480_PARAM_PULSE12V_VALUE_512us;
	dev->config.pulse5v          = DS2480_PARAM_PULSE5V_VALUE_524ms;
	dev->config.write1low        = DS2480_PARAM_WRITE1LOW_VALUE_8us;
	dev->config.sampleoffset     = DS2480_PARAM_SAMPLEOFFSET_VALUE_8us;
	dev->config.activepulluptime = DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_3p0us;
	dev->config.baudrate         = DS2480_PARAM_BAUDRATE_VALUE_9600;

	one_wire_bus_member_init(&dev->bus_master);
	one_wire_bus_member_master_set(&dev->bus_master);

	dev->bus_master.device = (void *)dev;
	dev->bus_master.driver = (void(*)(void *))ds2480b_dev_power_on;
#ifdef DEBUG
	strncpy(dev->bus_master.name, "ds2480b", sizeof dev->bus_master.name);
#endif
}

int
ds2480b_dev_bus_connect(struct ds2480b_device *dev, struct one_wire_bus *bus)
{
	if (ds2480b_dev_bus_connected(dev) != 0)
		return -1;

	return one_wire_bus_member_add(&dev->bus_master, bus);
}

int
ds2480b_dev_bus_connected(struct ds2480b_device *dev)
{
	return !list_empty(&dev->bus_master.list_entry);
}

int
ds2480b_dev_bus_rx_bit(struct ds2480b_device *dev)
{
	assert(dev != NULL);
	assert(ds2480b_dev_bus_connected(dev));
	return one_wire_bus_member_rx_bit(&dev->bus_master);
}

int
ds2480b_dev_bus_rx_byte(struct ds2480b_device *dev)
{
	return one_wire_bus_member_rx_byte(&dev->bus_master);
}

int
ds2480b_dev_bus_tx_bit(struct ds2480b_device *dev, int bit)
{
	assert(ds2480b_dev_bus_connected(dev));
	return one_wire_bus_member_tx_bit(&dev->bus_master, bit);
}

int
ds2480b_dev_bus_tx_byte(struct ds2480b_device *dev, int byte)
{
	return one_wire_bus_member_tx_byte(&dev->bus_master, byte);
}

void ds2480b_dev_connect_serial(struct ds2480b_device *dev, struct transport *t)
{
	dev->serial = t;
}

void ds2480b_dev_reset(struct ds2480b_device *dev, int speed)
{
	assert(dev != NULL);
	assert(speed >= 0 && speed < 3);
	assert(dev->mode == DS2480_MODE_COMMAND ||
	       dev->mode == DS2480_MODE_CHECK);

	dev->mode  = DS2480_MODE_COMMAND;
	dev->speed = speed;

	dev->config.slew             = DS2480_PARAM_SLEW_VALUE_15Vus;
	dev->config.pulse12v         = DS2480_PARAM_PULSE12V_VALUE_512us;
	dev->config.pulse5v          = DS2480_PARAM_PULSE5V_VALUE_524ms;
	dev->config.write1low        = DS2480_PARAM_WRITE1LOW_VALUE_8us;
	dev->config.sampleoffset     = DS2480_PARAM_SAMPLEOFFSET_VALUE_8us;
	dev->config.activepulluptime = DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_3p0us;
	dev->config.baudrate         = DS2480_PARAM_BAUDRATE_VALUE_9600;
}

static inline int
__ds2480b_speed_parse(int speed)
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

int ds2480b_dev_config_read(struct ds2480b_device *dev, int param)
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

int ds2480b_dev_config_write(struct ds2480b_device *dev, int param, int value)
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
ds2480b_dev_command_configuration(struct ds2480b_device *dev, unsigned char byte)
{
	int param_code, param_value;
	unsigned char reply;

	param_code  = (byte >> 4) & 7;
	param_value = (byte >> 1) & 7;

	DEBUG_LOG("  Configuration param: %s (0x%.2x) value: %s (0x%.2x)\n",
		__param_code_names[param_code],
		param_code,
		param_code == DS2480_PARAM_PARMREAD ?
			__param_code_names[param_value] :
			__param_value_names[param_code][param_value],
		param_value
	);

	/* param_code is 0 for read requests, and param_value will select the
	 * parameter to read.
	 *
	 * param_code will select the parameter to write otherwise, and
	 * param_value is the value to write.
	 */
	if (param_code == DS2480_PARAM_PARMREAD) {
		param_code  = param_value;
		param_value = ds2480b_dev_config_read(dev, param_code);
		reply = param_value << 1;
		DEBUG_LOG("  PARMREAD: %s (0x%.2x)\n",
			__param_value_names[param_code][param_value],
			param_value
		);
	} else {
		ds2480b_dev_config_write(dev, param_code, param_value);
		reply = byte & ~1;
	}

	return reply;
}

static int
ds2480b_dev_command_reset(struct ds2480b_device *dev, unsigned char byte)
{
	assert( (byte & 0xE3) == 0xC1);

	dev->speed = __ds2480b_speed_parse( (byte >> 2) & 3);
	DEBUG_LOG("    speed: %s (%d)\n", __speed_names[dev->speed], dev->speed);

	/* XXX: bit of a hack, command reset should not reset the ds2480b
	 * but instead generate a reset pulse.  This is just for the master
	 * reset.
	 */
	ds2480b_dev_reset(dev, dev->speed);

	one_wire_bus_member_reset_pulse(&dev->bus_master);

	/* Reset response:
	 * 
	 * +-----------------------------------------------------------------+
	 * | BIT 7 | BIT 6 | BIT 5 | BIT 4 | BIT 3 | BIT 2 | BIT 1 | BIT 0   |
	 * +-------+-------+-------+-----------------------+-----------------+
	 * |   1   |   1   |   X   |   0   |   1   |   1   |      state      |
	 * +-------+-------+-------+-----------------------+-----------------+
	 *
	 * state 00: 1-Wire shorted
	 *       01: presence pulse
	 *       10: alarming presence pulse
	 *       11: no presence pulse
	 */

	return 0xcd;
}

static int
ds2480b_dev_command_search_accel(struct ds2480b_device *dev, unsigned char byte)
{
	assert(dev != NULL);
	assert( (byte & 2) == 0);

	dev->accelerator = (byte >> 4) & 1;
	dev->speed       = __ds2480b_speed_parse( (byte >> 2) & 3);
	DEBUG_LOG("    speed: %s (%d)\n", __speed_names[dev->speed], dev->speed);
	DEBUG_LOG("    accel: %d\n", dev->accelerator);

	/* No response. */
	return -2;
}

static int
ds2480b_dev_command_single_bit(struct ds2480b_device *dev, unsigned char byte)
{
	int bit, pullup, value;

	pullup = (byte >> 1) & 1;
	value  = (byte >> 4) & 1;

	dev->speed = __ds2480b_speed_parse( (byte >> 2) & 3);

	DEBUG_LOG("    speed: %s (%d)\n", __speed_names[dev->speed], dev->speed);
	DEBUG_LOG("    value: %d pullup: %d\n", value, pullup);

	bit = one_wire_bus_member_tx_bit(&dev->bus_master, value);

	/* Single bit response:
	 * 
	 * +-----------------------------------------------------------------+
	 * | BIT 7 | BIT 6 | BIT 5 | BIT 4 | BIT 3 | BIT 2 | BIT 1 | BIT 0   |
	 * +-------+-------+-------+-----------------------+-----------------+
	 * |   1   |   0   |   0   |      same as sent     | 1-wire bit (2x) |
	 * +-------+-------+-------+-----------------------+-----------------+
	 */
	return (byte & 0xfc) | bit << 1 | bit;
}

static int
ds2480b_dev_command_mode(struct ds2480b_device *dev, unsigned char byte)
{
	assert(dev != NULL);
	assert(dev->mode == DS2480_MODE_COMMAND);

	/* All command codes have their least significant bit set. */
	if ( (byte & 1) == 0)
		return -1;

	/* Data mode switch. */
	if (byte == 0xE1) {
		DEBUG_LOG("[DS2480] MODE_COMMAND -> MODE_DATA\n");
		dev->mode = DS2480_MODE_DATA;
		return -2;
	}

	/* Configuration command. */
	if ( (byte & DS2480_COMMAND_MASK) == DS2480_CONFIG)
		return ds2480b_dev_command_configuration(dev, byte);

	assert((byte & DS2480_COMMAND_MASK) == DS2480_COMMAND);
	DEBUG_LOG("  Command: %s (%u)\n", __command_names[byte >> 5], byte >> 5);

	/* Otherwise we use the top 3-bits to specify the operation. */
	switch (byte >> 5) {
	case DS2480_COMMAND_SINGLE_BIT:
		return ds2480b_dev_command_single_bit(dev, byte);
	case DS2480_COMMAND_SEARCH_ACCELERATOR_CONTROL:
		return ds2480b_dev_command_search_accel(dev, byte);
	case DS2480_COMMAND_RESET:
		return ds2480b_dev_command_reset(dev, byte);
	case DS2480_COMMAND_PULSE:
		assert( ((byte >> 2) & 3) == 3);
		int arm_pullup = (byte >> 1) & 1;
		int pulse_type = (byte >> 4) & 1;
		DEBUG_LOG("    arm_pullup: %d pulse_type: %d\n", arm_pullup, pulse_type);
		return -1;
	}

	return -1;
}

static int
ds2480b_dev_data_mode(struct ds2480b_device *dev, uint8_t byte, int checked)
{
	assert(dev != NULL);

	if (checked == 0 && byte == 0xE3) {
		DEBUG_LOG("[DS2480] MODE_DATA -> MODE_CHECK\n");
		dev->mode = DS2480_MODE_CHECK;
		return -2;
	}

	/* Special case; we perform the search here. */
	if (dev->accelerator) {
		uint8_t search[16];
		uint8_t response[16] = {0};

		/* We receive 16 bytes from serial for the search. */
		search[0] = byte;
		if (transport_read_all(dev->serial, &search[1], 15) == -1)
			return -1;

		for (int i = 0; i < 64; i++) {
			int b1 = ds2480b_dev_bus_rx_bit(dev);
			int b2 = ds2480b_dev_bus_rx_bit(dev);

#if 0
			/* Conflict */
			if (b1 == b2) {
				DEBUG_LOG("conflict: %d\n", b1);
				b1 = (search[i * 2 / 8] >> (i % 8 + 1)) & 1;
				response[i * 2 / 8] |= 1 << (i % 8);
			}
#endif

			printf("b1: %d\n", b1);
			response[i * 2 / 8] |= b1 << ((i * 2 % 8) + 1);
			ds2480b_dev_bus_tx_bit(dev, b1);
			printf("TXed rom bit #%d\n", i);
		}

		DEBUG_LOG("RESPONSE: ");
		for (int i = 0; i < 16; i++)
			DEBUG_LOG("%.2x", ((uint8_t *)response)[i]);
		DEBUG_LOG("\n");

		DEBUG_LOG("writing response ...\n");
//		if (transport_write_all(dev->serial, &byte, 1) == -1)
//			return -1;

		if (transport_write_all(dev->serial, response, 16) == -1)
			return -1;

		return -2;
	}

	return ds2480b_dev_bus_tx_byte(dev, byte);
}

static int
ds2480b_dev_check_mode(struct ds2480b_device *dev, unsigned char byte)
{
	assert(dev != NULL);

	if (byte == 0xE3) {
		DEBUG_LOG("[DS2480] MODE_CHECK -> MODE_DATA\n");
		dev->mode = DS2480_MODE_DATA;
		return ds2480b_dev_data_mode(dev, byte, 1);
	}

	/* This is a command mode request.  Switch over. */
	DEBUG_LOG("[DS2480] MODE_CHECK -> MODE_COMMAND\n");
	dev->mode = DS2480_MODE_COMMAND;
	return ds2480b_dev_command_mode(dev, byte);
}

static inline int __is_reset(unsigned char byte)
{
	/* COMMAND RESET with 9600 speed -> 0b11000001 */
	return (byte & 0xE3) == 0xC1;
}

int ds2480b_dev_power_on(struct ds2480b_device *dev)
{
	unsigned char request;
	int response;

	assert(dev != NULL);
	assert(dev->mode == DS2480_MODE_INACTIVE);

	DEBUG_LOG("[ds2480b] power on\n");

	/* Handle the calibration byte. */
	if (transport_read_all(dev->serial, &request, 1) == -1)
		return -1;

	DEBUG_LOG("    reset pulse: %.2x\n", request);

	/* We expect a reset command, but will not respond. */
	/* XXX: this is supposed to be 9600 bps.  Add check later. */
	if (!__is_reset(request))
		return -1;

	dev->mode = DS2480_MODE_COMMAND;

	while (dev->mode != DS2480_MODE_INACTIVE) {
		if (transport_read_all(dev->serial, &request, 1) == -1)
			return -1;

		DEBUG_LOG("[ds2480b] mode: %d command: %.2x\n", dev->mode, request);

		switch (dev->mode) {
		case DS2480_MODE_COMMAND:
			response = ds2480b_dev_command_mode(dev, request);
			break;
		case DS2480_MODE_DATA:
			response = ds2480b_dev_data_mode(dev, request, 0);
			break;
		case DS2480_MODE_CHECK:
			response = ds2480b_dev_check_mode(dev, request);
			break;
		default:
			response = -1;
			break;
		}

		if (response == -1)
			return -1;

		/* We have a real response, and we're still active, so we'll
		 * reply on the bus.
		 */
		if (response >= 0 && dev->mode != DS2480_MODE_INACTIVE) {
			unsigned char res = (unsigned char)response;

			if (transport_write_all(dev->serial, &res, 1) == -1)
				return -1;
		}
	}

	abort();

	return 0;
}
