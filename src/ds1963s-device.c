/* ds1963s-device.c
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ds1963s-common.h"
#include "ds1963s-device.h"
#include "getput.h"
#include "sha1.h"

#define DS1963S_TX_BIT(dev, bit)					\
	({								\
		int v;							\
									\
		v = one_wire_bus_member_tx_bit(&dev->bus_slave, bit);	\
		if (v == ONE_WIRE_BUS_SIGNAL_RESET) {			\
			DEBUG_LOG("[ds1963s] got reset pulse\n");	\
			dev->state = DS1963S_STATE_RESET;		\
			return ONE_WIRE_BUS_SIGNAL_RESET;		\
		}							\
									\
		v;							\
	})

#define DS1963S_TX_BYTE(dev, byte)					\
	({								\
		int v;							\
									\
		v = one_wire_bus_member_tx_byte(&dev->bus_slave, byte);	\
		if (v == ONE_WIRE_BUS_SIGNAL_RESET) {			\
			DEBUG_LOG("[ds1963s] got reset pulse\n");	\
			dev->state = DS1963S_STATE_RESET;		\
			return ONE_WIRE_BUS_SIGNAL_RESET;		\
		}							\
									\
		v;							\
	})

#define DS1963S_RX_BIT(dev)	DS1963S_TX_BIT(dev, 1)
#define DS1963S_RX_BYTE(dev)	DS1963S_TX_BYTE(dev, 0xFF)

/* Calculate the value for MPX, as used in __sha1_get_input_1. */
static uint8_t __mpx_get(int M, int X, uint8_t *SP)
{
	return (M << 7) | (X << 6) | (SP[12] & 0x3F);
}

/* Calculate the value for MP, as used in __sha1_get_input_2. */
static uint8_t __mp_get(int M, int X, int page)
{
	return (M << 7) | (X << 6) | (page & 0xF);
}

/* Construct the SHA-1 input for the following operations:
 *
 * - Validate Data Page
 * - Sign Data Page
 * - Authenticate Host
 * - Compute First Secret
 * - Compute Next Secret
 */
static void __sha1_get_input_1(uint8_t M[64], uint8_t *SS, uint8_t *PP,
                               uint8_t MPX, uint8_t *SP)
{
	memcpy(&M[ 0], &SS[ 0],  4);
	memcpy(&M[ 4], &PP[ 0], 32);
	memcpy(&M[36], &SP[ 8],  4);
	M[40] = MPX;
	memcpy(&M[41], &SP[13],  7);
	memcpy(&M[48], &SS[ 4],  4);
	memcpy(&M[52], &SP[20],  3);
	M[55] = 0x80;
	M[56] = 0;
	M[57] = 0;
	M[58] = 0;
	M[59] = 0;
	M[60] = 0;
	M[61] = 0;
	M[62] = 1;
	M[63] = 0xB8;
}

/* Construct the SHA-1 input for the following operations:
 *
 * - Read Authenticated Page
 * - Compute Challenge
 */
static void __sha1_get_input_2(uint8_t M[64], uint8_t *SS, uint8_t *CC,
                               uint8_t *PP, uint8_t FAMC, uint8_t MP,
                               uint8_t *SN, uint8_t *SP)
{
	memcpy(&M[ 0], &SS[ 0],  4);
	memcpy(&M[ 4], &PP[ 0], 32);
	memcpy(&M[36], &CC[ 0],  4);
	M[40] = MP;
	M[41] = FAMC;
	memcpy(&M[42], &SN[ 0],  6);
	memcpy(&M[48], &SS[ 4],  4);
	memcpy(&M[52], &SP[20],  3);
	M[55] = 0x80;
	M[56] = 0;
	M[57] = 0;
	M[58] = 0;
	M[59] = 0;
	M[60] = 0;
	M[61] = 0;
	M[62] = 1;
	M[63] = 0xB8;
}

/* Construct the SHA-1 output for all operation except:
 *
 * - Compute First Secret
 * - Compute Next Secret
 */
static void __sha1_get_output_1(uint8_t SP[32], uint32_t A, uint32_t B,
                                uint32_t C, uint32_t D, uint32_t E)
{
	SP[ 8] = (E >>  0) & 0xFF;
	SP[ 9] = (E >>  8) & 0xFF;
	SP[10] = (E >> 16) & 0xFF;
	SP[11] = (E >> 24) & 0xFF;

	SP[12] = (D >>  0) & 0xFF;
	SP[13] = (D >>  8) & 0xFF;
	SP[14] = (D >> 16) & 0xFF;
	SP[15] = (D >> 24) & 0xFF;

	SP[16] = (C >>  0) & 0xFF;
	SP[17] = (C >>  8) & 0xFF;
	SP[18] = (C >> 16) & 0xFF;
	SP[19] = (C >> 24) & 0xFF;

	SP[20] = (B >>  0) & 0xFF;
	SP[21] = (B >>  8) & 0xFF;
	SP[22] = (B >> 16) & 0xFF;
	SP[23] = (B >> 24) & 0xFF;

	SP[24] = (A >>  0) & 0xFF;
	SP[25] = (A >>  8) & 0xFF;
	SP[26] = (A >> 16) & 0xFF;
	SP[27] = (A >> 24) & 0xFF;
}

void ds1963s_dev_init(struct ds1963s_device *ds1963s)
{
	memset(ds1963s, 0, sizeof *ds1963s);

	ds1963s->family = DS1963S_DEVICE_FAMILY;
	memset(ds1963s->data_memory,   0xaa, sizeof ds1963s->data_memory);
	memset(ds1963s->secret_memory, 0xaa, sizeof ds1963s->secret_memory);
	memset(ds1963s->scratchpad,    0xff, sizeof ds1963s->scratchpad);

	strcpy(ds1963s->data_memory, "Hello!");

	one_wire_bus_member_init(&ds1963s->bus_slave);
        ds1963s->bus_slave.device = (void *)ds1963s;
        ds1963s->bus_slave.driver = (void(*)(void *))ds1963s_dev_power_on;

#ifdef DEBUG
	strncpy(ds1963s->bus_slave.name, "ds1963s", sizeof ds1963s->bus_slave.name);
#endif
}

void ds1963s_dev_destroy(struct ds1963s_device *ds1963s)
{
	assert(ds1963s != NULL);

	list_del(&ds1963s->bus_slave.list_entry);
	memset(ds1963s, 0, sizeof *ds1963s);
}

uint16_t
ds1963s_dev_ta_to_address(struct ds1963s_device *ds1963s)
{
	return ds1963s_ta_to_address(ds1963s->TA1, ds1963s->TA2);
}

void
ds1963s_dev_rom_code_get(struct ds1963s_device *ds1963s, uint8_t buf[8])
{
	buf[0] = ds1963s->family;
	memcpy(&buf[1], ds1963s->serial, 6);
	buf[7] = ds1963s_crc8(buf, 7);
}

void ds1963s_dev_connect_bus(struct ds1963s_device *ds1963s, struct one_wire_bus *bus)
{
	assert(ds1963s != NULL);

	one_wire_bus_member_add(bus, &ds1963s->bus_slave);
}

void ds1963s_dev_erase_scratchpad(struct ds1963s_device *ds1963s, int address)
{
	assert(ds1963s != NULL);
	assert(address >= 0 && address <= 65536);

	ds1963s->CHLG = 0;
	ds1963s->AUTH = 0;
	ds1963s->TA1  = address & 0xFF;
	ds1963s->TA2  = (address >> 8) & 0xFF;

	memset(ds1963s->scratchpad, 0xFF, sizeof ds1963s->scratchpad);

	ds1963s->HIDE = 0;
}

void ds1963s_dev_write_scratchpad(
	struct ds1963s_device *ds1963s,
	int address,
	const unsigned char *data,
	size_t len
) {
	assert(ds1963s != NULL);
	assert(address >= 0 && address <= 65536);
	assert(data != NULL);

	ds1963s->CHLG = 0;
	ds1963s->AUTH = 0;
	ds1963s->TA1  = address & 0xFF;
	ds1963s->TA2  = (address >> 8) & 0xFF;

	if (ds1963s->HIDE == 1) {
		/* XXX: test for secret address. */

		/* PF := 0; AA := 0 */
		ds1963s->ES &= 0x5F;

		/* T2:T0 := 0,0,0 */
		ds1963s->TA1 &= 0xF8;

		/* E4:E0 := T4, T3, 0, 0, 0 */
		ds1963s->ES &= 0xE0;                /* E4:E0 := 0,0,0,0,0 */
		ds1963s->ES |= ds1963s->TA1 & 0x1F; /* E4:E0 := T4:T0     */
		ds1963s->ES |= 0x07;                /* E2:E0 := 1,1,1     */
	} else {

	}
}

void ds1963s_dev_read_auth_page(struct ds1963s_device *ds1963s, int page)
{
	uint8_t M[64];
	uint8_t CC[4];
	SHA1_CTX ctx;

	/* XXX: set properly. */
	ds1963s->M = 0;

	ds1963s->X = 0;
	ds1963s->CHLG = 0;
	ds1963s->AUTH = 0;

	PUT_32BIT_LSB(CC, ds1963s->data_wc[page]);

	__sha1_get_input_2(
		M,
		&ds1963s->secret_memory[(page % 8) * 8],
		CC,
		&ds1963s->data_memory[(page % 16) * 32],
		DS1963S_DEVICE_FAMILY,
		__mp_get(ds1963s->M, ds1963s->X, page),
		ds1963s->serial,
		ds1963s->scratchpad
	);

	/* We omit the finalize, as the DS1963S does not use it, but rather
	 * uses the internal state A, B, C, D, E for the result.  This also
	 * means we have to subtract the initial state from the context, as
	 * it has added these to the results.
	 */
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, M, sizeof M);

	/* Write the results to the scratchpad. */
	__sha1_get_output_1(
		ds1963s->scratchpad, 
		ctx.state[0] - 0x67452301,
		ctx.state[1] - 0xEFCDAB89,
		ctx.state[2] - 0x98BADCFE,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);
}

int ds1963s_dev_compute_sha(struct ds1963s_device *ds1963s, int control)
{
	assert(ds1963s != NULL);

	switch (control) {
	case 0x0F:
		/* compute first secret */
		break;
	case 0xF0:
		/* compute next secret */
		break;
	case 0x3C:
		/* validate data page */
		break;
	case 0xC3:
		ds1963s_dev_sign_data_page(ds1963s);
		break;
	case 0xCC:
		/* compute challenge */
		break;
	case 0xAA:
		/* authenticate host */
		break;
	default:
		return -1;
	}

	return 0;
}

/* There is no page argument as this function only works on page 0 and 8.
 * These pages are aliased, with page-8 being the write-cycle-counter
 * updating version of page 0.  As there are no writes this is irrelevant.
 */
void ds1963s_dev_sign_data_page(struct ds1963s_device *ds1963s)
{
	uint8_t M[64];
	SHA1_CTX ctx;

	/* XXX: set properly. */
	ds1963s->M = 0;

	ds1963s->X = 0;
	ds1963s->CHLG = 0;
	ds1963s->AUTH = 0;

	__sha1_get_input_1(
		M,
		ds1963s->secret_memory,
		ds1963s->data_memory,
		__mpx_get(ds1963s->M, ds1963s->X, ds1963s->scratchpad),
		ds1963s->scratchpad
	);

	/* We omit the finalize, as the DS1963S does not use it, but rather
	 * uses the internal state A, B, C, D, E for the result.  This also
	 * means we have to subtract the initial state from the context, as
	 * it has added these to the results.
	 */
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, M, sizeof M);

	/* Write the results to the scratchpad. */
	__sha1_get_output_1(
		ds1963s->scratchpad, 
		ctx.state[0] - 0x67452301,
		ctx.state[1] - 0xEFCDAB89,
		ctx.state[2] - 0x98BADCFE,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);
}

int ds1963s_dev_rom_command_search_rom(struct ds1963s_device *dev)
{
	uint8_t rom_code[8];

	ds1963s_dev_rom_code_get(dev, rom_code);

	for (int i = 0; i < sizeof(rom_code); i++) {
		for (int j = 0; j < 8; j++) {
			int mb;
			int b1 = (rom_code[i] >> j) & 1;
			int b2 = !b1;

			DS1963S_TX_BIT(dev, b1);
			DS1963S_TX_BIT(dev, b2);
			mb = DS1963S_RX_BIT(dev);
			DEBUG_LOG("search rom bit #%d: %d\n", i * 8 + j, mb);

			/* Special case.  On mismatch we wait for reset. */
			if (mb != b1) {
				DEBUG_LOG("mismatch: expected %d got %d\n", b1, mb);
				dev->state = DS1963S_STATE_RESET_WAIT;
				return -1;
			}
		}
	}

	DEBUG_LOG("search rom match\n");

	return 0;
}

int
ds1963s_dev_rom_function(struct ds1963s_device *dev)
{
	int byte;

	byte = DS1963S_RX_BYTE(dev);

	switch (byte) {
	case 0x3C:
		DEBUG_LOG("[ds1963s|ROM] Overdrive skip ROM\n");
		dev->OD = 1;
		break;
	case 0xF0:
		DEBUG_LOG("[ds1963s|ROM] Search ROM\n");
		if (ds1963s_dev_rom_command_search_rom(dev) != 0)
			break;

		dev->state = DS1963S_STATE_MEMORY_FUNCTION;
		break;
	default:
		DEBUG_LOG("[ds1963s|ROM] Unknown command %.2x\n", byte);
		break;
	}

	return 0;
}

int
ds1963s_dev_memory_command_read_memory(struct ds1963s_device *dev)
{
	uint8_t  TA1, TA2;
	uint16_t addr;
	int page;

	dev->CHLG = 0;
	dev->AUTH = 0;

	TA1  = DS1963S_RX_BYTE(dev);
	TA2  = DS1963S_RX_BYTE(dev);
	addr = ds1963s_ta_to_address(TA1, TA2);

	while (addr < 0x2B0) {
		ds1963s_address_to_ta(addr, &dev->TA1, &dev->TA2);

		page = ds1963s_address_to_page(addr);
		if (page == 16 || page == 17) {
			DS1963S_TX_BYTE(dev, 0xFF);
		} else {
			DS1963S_TX_BYTE(dev, dev->memory[addr]);
		}

		/* DS1963S does not increment addr past 0x2AF. */
		if (addr == 0x2AF) break;
		addr += 1;
	}

	while (1)
		DS1963S_TX_BIT(dev, 1);
}

int
ds1963s_dev_memory_function(struct ds1963s_device *dev)
{
	int byte;

	byte = DS1963S_RX_BYTE(dev);

	switch (byte) {
	case 0xF0:
		DEBUG_LOG("[ds1963s|MEMORY] Read Memory\n");
		ds1963s_dev_memory_command_read_memory(dev);
		break;
	default:
		DEBUG_LOG("[ds1963s|MEMORY] Unknown command %.2x\n", byte);
		break;
	}

	return 0;
}

int ds1963s_dev_power_on(struct ds1963s_device *dev)
{
        assert(dev != NULL);

	while (1) {
		switch(dev->state) {
		case DS1963S_STATE_INITIAL:
			DEBUG_LOG("[ds1963s|INITIAL] power on\n");
			dev->state = DS1963S_STATE_RESET_WAIT;
			break;
		case DS1963S_STATE_RESET_WAIT:
			DEBUG_LOG("[ds1963s|RESET_WAIT] waiting on reset\n");
			/* We ignore things until we see a reset pulse. */
			while (one_wire_bus_member_rx_bit(&dev->bus_slave) != -1)
				break;

			DEBUG_LOG("[ds1963s|RESET_WAIT] Received reset pulse...\n");
			dev->state = DS1963S_STATE_RESET;
			break;
		case DS1963S_STATE_RESET:

#if 0			/* XXX: hack */
			ds1963s_dev_rx_bit(dev);
			DEBUG_LOG("[ds1963s|RESET] Sending presence pulse\n");
			ds1963s_dev_tx_bit(dev, 1);
#endif
			dev->state = DS1963S_STATE_ROM_FUNCTION;
			break;
		case DS1963S_STATE_ROM_FUNCTION:
			ds1963s_dev_rom_function(dev);
			break;
		case DS1963S_STATE_MEMORY_FUNCTION:
			ds1963s_dev_memory_function(dev);
			break;
		}
	}

	return 0;
}

#ifdef DS1963S_DEVICE_TEST
int main(void)
{
	struct ds1963s_device ds1963s;

	memset(&ds1963s, 0, sizeof ds1963s);
	memset(ds1963s.scratchpad, 0xff, sizeof ds1963s.scratchpad);
	memcpy(ds1963s.secret_memory, "\x00\x00\x01\x00\x01\x05\x07\x00", 8);

	ds1963s_sign_page(&ds1963s);
	int i;

	for (i = 0; i < 32; i++)
		printf("%.2x", ds1963s.scratchpad[i]);
	printf("\n");

	/* Test initialization for read auth page. */
	memset(&ds1963s, 0, sizeof ds1963s);
	memset(ds1963s.scratchpad, 0xff, sizeof ds1963s.scratchpad);
	ds1963s.data_wc[0] = 1;
	memcpy(ds1963s.serial, "\x8e\x4f\x68\x00\x00\x00", 6);
	memcpy(ds1963s.secret_memory, "\x00\x00\x01\x00\x01\x05\x07\x00", 8);
	ds1963s_read_auth_page(&ds1963s, 0);

	for (i = 0; i < 32; i++)
		printf("%.2x", ds1963s.scratchpad[i]);
	printf("\n");
}
#endif
