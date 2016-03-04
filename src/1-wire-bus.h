#ifndef __1_WIRE_BUS_H
#define __1_WIRE_BUS_H

#include "list.h"

struct one_wire_slave
{
	struct list_head list_entry;
	int (*rx_bit)(void);
	int (*tx_bit)(int bit);
};

struct one_wire_bus
{
	struct list_head slaves;
};

#ifdef __cplusplus
extern "C" {
#endif

void one_wire_bus_init(struct one_wire_bus *bus);

void one_wire_bus_slave_add(struct one_wire_bus *bus, struct one_wire_slave *slave);
void one_wire_bus_slave_remove(struct one_wire_bus *bus, struct one_wire_slave *slave);

int  one_wire_bus_tx_bit(struct one_wire_bus *bus, int bit);
int  one_wire_bus_rx_bit(struct one_wire_bus *bus);
int  one_wire_bus_rx_byte(struct one_wire_bus *bus);
int  one_wire_bus_tx_byte(struct one_wire_bus *bus, int byte);

#ifdef __cplusplus
};
#endif

#endif
