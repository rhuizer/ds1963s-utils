/* 1-wire-bus.c
 *
 * A virtual 1-wire bus implementation.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 *  -- Ronald Huizer, (C) 2016
 */
#include <assert.h>
#include "1-wire-bus.h"

void one_wire_bus_init(struct one_wire_bus *bus)
{
	list_init(&bus->slaves);
}

void one_wire_bus_slave_add(struct one_wire_bus *bus, struct one_wire_slave *slave)
{
	assert(bus != NULL);
	assert(slave != NULL);
	list_add(&slave->list_entry, &bus->slaves);
}

void one_wire_bus_slave_remove(struct one_wire_bus *bus, struct one_wire_slave *slave)
{
	assert(bus != NULL);
	assert(slave != NULL);
	list_del(&slave->list_entry);
}

int one_wire_bus_tx_bit(struct one_wire_bus *bus, int bit)
{
	struct list_head *lh;

	assert(bus != NULL);

	list_for_each (lh, &bus->slaves) {
		struct one_wire_slave *slave;

		slave = list_entry(lh, struct one_wire_slave, list_entry);
		if (slave->tx_bit != NULL)
			slave->tx_bit(bit);
	}

	/* XXX: deal with communication errors later. */
	return 0;
}

int one_wire_bus_rx_bit(struct one_wire_bus *bus)
{
	struct list_head *lh;
	int bit = 1;

	assert(bus != NULL);
	assert(!list_empty(&bus->slaves));

	list_for_each (lh, &bus->slaves) {
		struct one_wire_slave *slave;

		slave = list_entry(lh, struct one_wire_slave, list_entry);
		if (slave->rx_bit != NULL)
			bit &= slave->rx_bit();	/* Collision is wired-AND. */
	}

	/* XXX: deal with communication errors later. */
	return 0;

}

int one_wire_bus_tx_byte(struct one_wire_bus *bus, int byte)
{
	int i;

	for (i = 0; i < 8; i++)
		one_wire_bus_tx_bit(bus, (byte >> i) & 1);

	return 0;
}

int one_wire_bus_rx_byte(struct one_wire_bus *bus)
{
	int result = 0;
	int i;

	for (i = 0; i < 8; i++)
		result |= one_wire_bus_rx_bit(bus) << i;

	return result;
}
