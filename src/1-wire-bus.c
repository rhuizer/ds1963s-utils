/* 1-wire-bus.c
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "1-wire-bus.h"

static inline int
__one_wire_bus_cycle(struct one_wire_bus *bus)
{
	struct list_head *lh;

	assert(bus != NULL);

	DEBUG_LOG("[1-wire-bus] New cycle\n");

	/* If we have no members, there is nothing to drive anything so we
	 * just terminate.
	 */
	if (list_empty(&bus->members))
		return -1;

	/* In a single bus cycle, we first iterate over all members that are
	 * not yet accessing the bus.  To keep every member in lock-step it
	 * is necessary to wait until every member is accessing the bus for
	 * either read or write operations.
	 */
	list_for_each (lh, &bus->members) {
		struct one_wire_bus_member *m =
			list_entry(lh, struct one_wire_bus_member, list_entry);

		if (m->bus_access != ONE_WIRE_BUS_ACCESS_NONE)
			continue;

		coroutine_yieldto(&bus->coro, &m->coro);
	}

	/* We now know that every bus member is either reading or writing the
	 * bus.  If they are writing they have yielded simply until the end of
	 * the current cycle.  Readers will have to be yielded to so they can
	 * process the bus input they received.
	 */
	list_for_each (lh, &bus->members) {
		struct one_wire_bus_member *m =
			list_entry(lh, struct one_wire_bus_member, list_entry);
		int bus_access = m->bus_access;

		assert(bus_access != ONE_WIRE_BUS_ACCESS_NONE);

		m->bus_access = ONE_WIRE_BUS_ACCESS_NONE;
		if (bus_access == ONE_WIRE_BUS_ACCESS_READ)
			coroutine_yieldto(&bus->coro, &m->coro);
	}

	return 0;
}

static void
__one_wire_bus_coroutine(struct coroutine *coro)
{
	struct one_wire_bus *bus;

	assert(coro != NULL);
	assert(coro->cookie != NULL);

	bus = (struct one_wire_bus *)coro->cookie;

	/* The bus keeps all attached members in lock-step by simply
	 * yielding to all of them once to simulate a bus cycle.
	 */
	while (__one_wire_bus_cycle(bus) == 0);
}

static void __one_wire_bus_member_coroutine(struct coroutine *coro)
{
	struct one_wire_bus_member *member;

	assert(coro != NULL);
	assert(coro->cookie != NULL);

	member = (struct one_wire_bus_member *)coro->cookie;
	member->driver(member->device);
}

void one_wire_bus_init(struct one_wire_bus *bus)
{
	assert(bus != NULL);

	memset(bus, 0, sizeof *bus);
	bus->signal = ONE_WIRE_BUS_SIGNAL_ZERO;
	list_init(&bus->members);
	coroutine_init(&bus->coro, __one_wire_bus_coroutine, bus);
}

void one_wire_bus_member_init(struct one_wire_bus_member *member)
{
	assert(member != NULL);

	memset(member, 0, sizeof *member);
	member->bus_access = ONE_WIRE_BUS_ACCESS_NONE;
	list_init(&member->list_entry);
}

int one_wire_bus_member_add(struct one_wire_bus *bus, struct one_wire_bus_member *member)
{
	assert(bus != NULL);
	assert(member != NULL);
	assert(list_empty(&member->list_entry));

	DEBUG_LOG("Adding member `%.*s' to the 1-wire bus.\n",
	          (int)sizeof(member->name), member->name);

	member->bus = bus;
	list_add(&member->list_entry, &bus->members);
	coroutine_init(&member->coro, __one_wire_bus_member_coroutine, member);

	return 0;
}

void one_wire_bus_member_tx_bit(struct one_wire_bus_member *member, int bit)
{
	assert(member != NULL);
	assert(member->bus != NULL);
	assert(bit == 0 || bit == 1);

	member->bus_access = ONE_WIRE_BUS_ACCESS_WRITE;

	if (bit == 0)
		member->bus->signal = ONE_WIRE_BUS_SIGNAL_ZERO;
	else
		member->bus->signal = ONE_WIRE_BUS_SIGNAL_ONE;

	coroutine_yieldto(&member->coro, &member->bus->coro);
}

int one_wire_bus_member_rx_bit(struct one_wire_bus_member *member)
{
	assert(member != NULL);
	assert(member->bus != NULL);

	member->bus_access = ONE_WIRE_BUS_ACCESS_READ;
	coroutine_yieldto(&member->coro, &member->bus->coro);

	if (member->bus->signal >= 0)
		return !member->bus->signal;

	return member->bus->signal;
}

void
one_wire_bus_member_reset_pulse(struct one_wire_bus_member *member)
{
	assert(member != NULL);
	assert(member->bus != NULL);

	member->bus_access  = ONE_WIRE_BUS_ACCESS_WRITE;
	member->bus->signal = ONE_WIRE_BUS_SIGNAL_RESET;
	coroutine_yieldto(&member->coro, &member->bus->coro);
}

int one_wire_bus_member_rx_byte(struct one_wire_bus_member *member)
{
	int result = 0;
	int bit;
	int i;

	for (i = 0; i < 8; i++) {
		bit = one_wire_bus_member_rx_bit(member);

		if (bit == ONE_WIRE_BUS_SIGNAL_RESET)
			return ONE_WIRE_BUS_SIGNAL_RESET;

		result |= bit << i;
	}

	return result;
}

void
one_wire_bus_member_tx_byte(struct one_wire_bus_member *member, int byte)
{
	int i;

	for (i = 0; i < 8; i++)
		one_wire_bus_member_tx_bit(member, (byte >> i) & 1);
}

int one_wire_bus_run(struct one_wire_bus *bus)
{
	return coroutine_main();
}
