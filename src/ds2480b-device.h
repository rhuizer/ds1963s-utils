/* ds2480b-device.h
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
#ifndef DS2480_DEVICE_H
#define DS2480_DEVICE_H

#include "1-wire-bus.h"

#define DS2480_COMMAND					0x81
#define DS2480_CONFIG					0x01
#define DS2480_COMMAND_MASK				0x81

#define DS2480_MODE_INACTIVE				0
#define DS2480_MODE_COMMAND				1
#define DS2480_MODE_DATA				2
#define DS2480_MODE_CHECK				3

#define DS2480_SPEED_REGULAR				0
#define DS2480_SPEED_FLEX				1
#define DS2480_SPEED_OVERDRIVE				2

#define DS2480_COMMAND_SINGLE_BIT			4
#define DS2480_COMMAND_SEARCH_ACCELERATOR_CONTROL	5
#define DS2480_COMMAND_RESET				6
#define DS2480_COMMAND_PULSE				7

#define DS2480_PARAM_PARMREAD				0
#define DS2480_PARAM_SLEW				1
#define DS2480_PARAM_12VPULSE				2
#define DS2480_PARAM_5VPULSE				3
#define DS2480_PARAM_WRITE1LOW				4
#define DS2480_PARAM_SAMPLEOFFSET			5
#define DS2480_PARAM_ACTIVEPULLUPTIME			6
#define DS2480_PARAM_BAUDRATE				7

#define DS2480_PARAM_SLEW_VALUE_15Vus			0
#define DS2480_PARAM_SLEW_VALUE_2p2Vus			1
#define DS2480_PARAM_SLEW_VALUE_1p65Vus			2
#define DS2480_PARAM_SLEW_VALUE_1p37Vus			3
#define DS2480_PARAM_SLEW_VALUE_1p1Vus			4
#define DS2480_PARAM_SLEW_VALUE_0p83Vus			5
#define DS2480_PARAM_SLEW_VALUE_0p7Vus			6
#define DS2480_PARAM_SLEW_VALUE_0p55Vus			7

#define DS2480_PARAM_PULSE12V_VALUE_32us		0
#define DS2480_PARAM_PULSE12V_VALUE_64us		1
#define DS2480_PARAM_PULSE12V_VALUE_128us		2
#define DS2480_PARAM_PULSE12V_VALUE_256us		3
#define DS2480_PARAM_PULSE12V_VALUE_512us		4
#define DS2480_PARAM_PULSE12V_VALUE_1024us		5
#define DS2480_PARAM_PULSE12V_VALUE_2048us		6
#define DS2480_PARAM_PULSE12V_VALUE_infinite		7

#define DS2480_PARAM_PULSE5V_VALUE_16p4ms		0
#define DS2480_PARAM_PULSE5V_VALUE_65p5ms		1
#define DS2480_PARAM_PULSE5V_VALUE_131ms		2
#define DS2480_PARAM_PULSE5V_VALUE_262ms		3
#define DS2480_PARAM_PULSE5V_VALUE_524ms		4
#define DS2480_PARAM_PULSE5V_VALUE_1p05s		5
#define DS2480_PARAM_PULSE5V_VALUE_2p10s		6
#define DS2480_PARAM_PULSE5V_VALUE_infinite		7

#define DS2480_PARAM_WRITE1LOW_VALUE_8us		0
#define DS2480_PARAM_WRITE1LOW_VALUE_9us		1
#define DS2480_PARAM_WRITE1LOW_VALUE_10us		2
#define DS2480_PARAM_WRITE1LOW_VALUE_11us		3
#define DS2480_PARAM_WRITE1LOW_VALUE_12us		4
#define DS2480_PARAM_WRITE1LOW_VALUE_13us		5
#define DS2480_PARAM_WRITE1LOW_VALUE_14us		6
#define DS2480_PARAM_WRITE1LOW_VALUE_15us		7

#define DS2480_PARAM_SAMPLEOFFSET_VALUE_3us		0
#define DS2480_PARAM_SAMPLEOFFSET_VALUE_4us		1
#define DS2480_PARAM_SAMPLEOFFSET_VALUE_5us		2
#define DS2480_PARAM_SAMPLEOFFSET_VALUE_6us		3
#define DS2480_PARAM_SAMPLEOFFSET_VALUE_7us		4
#define DS2480_PARAM_SAMPLEOFFSET_VALUE_8us		5
#define DS2480_PARAM_SAMPLEOFFSET_VALUE_9us		6
#define DS2480_PARAM_SAMPLEOFFSET_VALUE_10us		7

#define DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_0p0us	0
#define DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_0p5us	1
#define DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_1p0us	2
#define DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_1p5us	3
#define DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_2p0us	4
#define DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_2p5us	5
#define DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_3p0us	6
#define DS2480_PARAM_ACTIVEPULLUPTIME_VALUE_3p5us	7

#define DS2480_PARAM_BAUDRATE_VALUE_9600		0
#define DS2480_PARAM_BAUDRATE_VALUE_19200		1
#define DS2480_PARAM_BAUDRATE_VALUE_57600		2
#define DS2480_PARAM_BAUDRATE_VALUE_115200		3

struct ds2480b_device_configuration
{
	int	slew;
	int	pulse12v;
	int	pulse5v;
	int	write1low;
	int	sampleoffset;
	int	activepulluptime;
	int	baudrate;
};

struct ds2480b_device
{
	int	accelerator;
	int	mode;
	int	speed;
	struct ds2480b_device_configuration config;

	/* 1-wire bus the ds2480b is the master of. */
	struct one_wire_bus_member bus_master;
	/* Host serial port the ds2480b communicated with. */
	struct transport *serial;
};

#ifdef __cplusplus
extern "C" {
#endif

void ds2480b_dev_init(struct ds2480b_device *);
int  ds2480b_dev_bus_connect(struct ds2480b_device *, struct one_wire_bus *);
int  ds2480b_dev_bus_connected(struct ds2480b_device *);
int  ds2480b_dev_bus_rx_bit(struct ds2480b_device *);
int  ds2480b_dev_bus_tx_bit(struct ds2480b_device *, int);

void ds2480b_dev_connect_serial(struct ds2480b_device *, struct transport *);
int  ds2480b_dev_power_on(struct ds2480b_device *dev);

#ifdef __cplusplus
};
#endif

#endif
