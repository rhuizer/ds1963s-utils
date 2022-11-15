/* ds1963s-device.h
 *
 * A software implementation of the DS1963S iButton.
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
#ifndef DS1963S_DEVICE_H
#define DS1963S_DEVICE_H

#include "1-wire-bus.h"

#define DS1963S_DEVICE_FAMILY		0x18

#define DS1963S_STATE_TERMINATED	-1
#define DS1963S_STATE_INITIAL		0
#define DS1963S_STATE_RESET_WAIT	1
#define DS1963S_STATE_RESET		2
#define DS1963S_STATE_ROM_FUNCTION	3
#define DS1963S_STATE_MEMORY_FUNCTION	4

struct ds1963s_device
{
	uint8_t		family;
	uint8_t		serial[6];

	union {
		uint8_t		memory[1024];

		struct {
			uint8_t		data_memory[512];
			uint8_t		secret_memory[64];
			uint8_t		scratchpad[32];
			uint32_t	data_wc[8];
			uint32_t	secret_wc[8];
			uint32_t	prng_counter;
		};
	};

	uint8_t		TA1;
	uint8_t		TA2;
	uint8_t		ES;

	uint8_t		M:1;
	uint8_t		X:1;
	uint8_t		HIDE:1;
	uint8_t		CHLG:1;
	uint8_t		AUTH:1;
	uint8_t		MATCH:1;
	uint8_t		OD:1;
	uint8_t		PF:1;
	uint8_t		RC:1;
	uint8_t		AA:1;

	int		state;

	/* The 1-wire bus the ds1963s is connected to as a slave. */
	struct one_wire_bus_member bus_slave;
};

#ifdef __cplusplus
extern "C" {
#endif

void     ds1963s_dev_init(struct ds1963s_device *dev);
void     ds1963s_dev_destroy(struct ds1963s_device *dev);
uint64_t ds1963s_rom_code_get(struct ds1963s_device *dev);

int   ds1963s_dev_pf_get(struct ds1963s_device *dev);
void  ds1963s_dev_pf_set(struct ds1963s_device *dev, int pf);

void ds1963s_dev_connect_bus(struct ds1963s_device *ds1963s, struct one_wire_bus *bus);
int  ds1963s_dev_power_on(struct ds1963s_device *ds1963s);

void ds1963s_dev_erase_scratchpad(struct ds1963s_device *ds1963s, int address);
void ds1963s_dev_read_auth_page(struct ds1963s_device *ds1963s, int page);
int  ds1963s_dev_sign_data_page(struct ds1963s_device *dev);
int  ds1963s_dev_validate_data_page(struct ds1963s_device *dev);

#ifdef __cplusplus
};
#endif

#endif
