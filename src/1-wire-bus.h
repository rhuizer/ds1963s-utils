/* 1-wire-bus.h
 *
 * A virtual 1-wire bus implementation.
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
#ifndef ONE_WIRE_BUS_H
#define ONE_WIRE_BUS_H

#include <inttypes.h>
#include "coroutine.h"
#include "debug.h"
#include "list.h"

/* Do not redefine these three, it will break things. */
#define ONE_WIRE_BUS_SIGNAL_TERMINATE	-2
#define ONE_WIRE_BUS_SIGNAL_RESET	-1
#define ONE_WIRE_BUS_SIGNAL_ZERO	0
#define ONE_WIRE_BUS_SIGNAL_ONE		1

#define ONE_WIRE_BUS_ACCESS_NONE	0
#define ONE_WIRE_BUS_ACCESS_READ	1
#define ONE_WIRE_BUS_ACCESS_WRITE	2

#define ONE_WIRE_BUS_STATE_RUNNING	0
#define ONE_WIRE_BUS_STATE_TERMINATED	1

#define ONE_WIRE_BUS_MEMBER_MASTER	0
#define ONE_WIRE_BUS_MEMBER_SLAVE	1

struct one_wire_bus;

typedef void (*bus_device_driver_t)(void *);

struct one_wire_bus_member
{
	int                  type;
	struct coroutine     coro;
	struct one_wire_bus *bus;
	struct list_head     list_entry;
	int	             bus_access;
	void *               device;
	bus_device_driver_t  driver;
#ifdef DEBUG
	char                 name[16];
#endif
};

struct one_wire_bus
{
	int              state;
	struct list_head members;
	struct coroutine coro;
	int              signal;
#ifdef DEBUG
	uint64_t         cycle;
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

void one_wire_bus_init(struct one_wire_bus *bus);
int  one_wire_bus_run(struct one_wire_bus *bus);

void one_wire_bus_member_init(struct one_wire_bus_member *member);
int  one_wire_bus_member_add(struct one_wire_bus_member *, struct one_wire_bus *);
void one_wire_bus_member_remove(struct one_wire_bus_member *);

void one_wire_bus_member_master_set(struct one_wire_bus_member *member);

void one_wire_bus_member_reset_pulse(struct one_wire_bus_member *);
int  one_wire_bus_member_tx_bit(struct one_wire_bus_member *, int);
int  one_wire_bus_member_rx_bit(struct one_wire_bus_member *);
int  one_wire_bus_member_rx_byte(struct one_wire_bus_member *);
int  one_wire_bus_member_tx_byte(struct one_wire_bus_member *, int);

void one_wire_bus_member_write_bit(struct one_wire_bus_member *member, int bit);
void one_wire_bus_member_write_byte(struct one_wire_bus_member *member, int byte);
int  one_wire_bus_member_read_bit(struct one_wire_bus_member *member, int bit);
int  one_wire_bus_member_read_byte(struct one_wire_bus_member *member, int byte);

#ifdef __cplusplus
};
#endif

#endif
