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

#ifndef DEBUG_BUS
#undef DEBUG_LOG
#define DEBUG_LOG(x, ...)
#endif

static inline int
__one_wire_bus_cycle(struct one_wire_bus *bus)
{
	struct list_head *lh, *lh2;
	int signal;

	assert(bus != NULL);

	DEBUG_LOG("[1-wire-bus] cycle #%"PRIu64"\n", bus->cycle++);

	/* If we have no members, there is nothing to drive anything so we
	 * just terminate.
	 */
	if (list_empty(&bus->members))
		return -1;

	/* Default to a 1 signalled by positive voltage. */
	signal = ONE_WIRE_BUS_SIGNAL_ONE;

	/* In a single bus cycle, we first iterate over all members that are
	 * not yet accessing the bus.  To keep every member in lock-step it
	 * is necessary to wait until every member is accessing the bus for
	 * either read or write operations.
	 */
	list_for_each_safe (lh, lh2, &bus->members) {
		int tmp;
		struct one_wire_bus_member *m =
			list_entry(lh, struct one_wire_bus_member, list_entry);

		/* If we are terminating, we send all members the signal. */
		if (bus->state == ONE_WIRE_BUS_STATE_TERMINATED) {
			bus->signal = ONE_WIRE_BUS_SIGNAL_TERMINATE;
			coroutine_yieldto(&bus->coro, &m->coro);
			continue;
		}

		if (m->bus_access != ONE_WIRE_BUS_ACCESS_NONE)
			continue;

		/* The 1-wire signal is a logical and over all devices on the
		 * line.  If one pulls the line low, it will be low.
		 */
		tmp = (int)(intptr_t)coroutine_await(&bus->coro, &m->coro);

		/* It's possible the bus member disappeared during our await,
		 * so we check if it is still present.  If not we skip
		 * updating the signal.
		 *
		 * XXX: maybe try to solve this in coroutine.c
		 */
		if (list_empty(&m->list_entry))
			continue;

		if (tmp == -1)
			signal = -1;
		else if (signal != -1)
			signal &= tmp;

		DEBUG_LOG("[1-wire-bus] %s signal %d\n", m->name, signal);
	}

	bus->signal = signal;
	DEBUG_LOG("[1-wire-bus] final bus signal %d\n", bus->signal);

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

static void
__one_wire_bus_member_coroutine(struct coroutine *coro)
{
	struct one_wire_bus_member *member;

	assert(coro != NULL);
	assert(coro->cookie != NULL);

	member = (struct one_wire_bus_member *)coro->cookie;
	member->driver(member->device);
}

static void
__one_wire_bus_member_destructor(struct coroutine *coro)
{
	struct one_wire_bus_member *member;

	assert(coro != NULL);
	assert(coro->cookie != NULL);
	member = (struct one_wire_bus_member *)coro->cookie;

	/* If the master goes, shut down the whole bus. */
	if (member->type == ONE_WIRE_BUS_MEMBER_MASTER)
		member->bus->state = ONE_WIRE_BUS_STATE_TERMINATED;

	one_wire_bus_member_remove(member);
}

void one_wire_bus_init(struct one_wire_bus *bus)
{
	assert(bus != NULL);

	memset(bus, 0, sizeof *bus);
	bus->state  = ONE_WIRE_BUS_STATE_TERMINATED;
	bus->signal = ONE_WIRE_BUS_SIGNAL_ONE;
	list_init(&bus->members);
	coroutine_init(&bus->coro, __one_wire_bus_coroutine, bus);
}

void one_wire_bus_member_init(struct one_wire_bus_member *member)
{
	assert(member != NULL);

	memset(member, 0, sizeof *member);
	member->type       = ONE_WIRE_BUS_MEMBER_SLAVE;
	member->bus_access = ONE_WIRE_BUS_ACCESS_NONE;
	list_init(&member->list_entry);
}

int
one_wire_bus_member_add(struct one_wire_bus_member *member,
                        struct one_wire_bus *bus)
{
	assert(bus != NULL);
	assert(member != NULL);
	assert(list_empty(&member->list_entry));

	DEBUG_LOG("[1-wire-bus] Adding member `%s'\n", member->name);

	member->bus = bus;
	list_add(&member->list_entry, &bus->members);
	coroutine_init(&member->coro, __one_wire_bus_member_coroutine, member);
	coroutine_destructor_set(&member->coro, __one_wire_bus_member_destructor);

	return 0;
}

void
one_wire_bus_member_master_set(struct one_wire_bus_member *member)
{
	assert(member != NULL);
	member->type = ONE_WIRE_BUS_MEMBER_MASTER;
}

void
one_wire_bus_member_remove(struct one_wire_bus_member *member)
{
	assert(member != NULL);
	assert(member->bus != NULL);
	assert(!list_empty(&member->list_entry));

	DEBUG_LOG("[1-wire-bus] Removing member `%s'\n", member->name);

	member->bus = NULL;
	list_del_init(&member->list_entry);
}

int
one_wire_bus_member_tx_bit(struct one_wire_bus_member *member, int bit)
{
	assert(member != NULL);
	assert(member->bus != NULL);
	assert(bit == 0 || bit == 1);

	coroutine_returnto(&member->coro, &member->bus->coro, (void *)(intptr_t)bit);
	DEBUG_LOG("[1-wire-bus] %s tx %d rx %d\n", member->name, bit, member->bus->signal);
	return member->bus->signal;
}

int
one_wire_bus_member_rx_bit(struct one_wire_bus_member *member)
{
	assert(member != NULL);
	assert(member->bus != NULL);

	coroutine_returnto(&member->coro, &member->bus->coro, (void *)1);

	DEBUG_LOG("[1-wire-bus] %s rx %d\n", member->name, member->bus->signal);

	return member->bus->signal;
}

void
one_wire_bus_member_reset_pulse(struct one_wire_bus_member *member)
{
	assert(member != NULL);
	assert(member->bus != NULL);

	coroutine_returnto(&member->coro, &member->bus->coro, (void *)-1);
	DEBUG_LOG("[1-wire-bus] %s tx reset pulse\n", member->name);
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

int
one_wire_bus_member_tx_byte(struct one_wire_bus_member *member, int byte)
{
	int result = 0;
	int bit;

	for (int i = 0; i < 8; i++) {
		bit = one_wire_bus_member_tx_bit(member, (byte >> i) & 1);

		if (bit == ONE_WIRE_BUS_SIGNAL_RESET)
			return ONE_WIRE_BUS_SIGNAL_RESET;

		result |= bit << i;
	}

	return result;
}

int one_wire_bus_run(struct one_wire_bus *bus)
{
	bus->state = ONE_WIRE_BUS_STATE_RUNNING;
	return coroutine_main();
}
