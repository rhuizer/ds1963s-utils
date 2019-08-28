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
#include <stdlib.h>
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
			__ds1963s_dev_do_reset_pulse(dev);		\
			return ONE_WIRE_BUS_SIGNAL_RESET;		\
		} else if (v == ONE_WIRE_BUS_SIGNAL_TERMINATE) {	\
			dev->state = DS1963S_STATE_TERMINATED;		\
			return ONE_WIRE_BUS_SIGNAL_TERMINATE;		\
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
			__ds1963s_dev_do_reset_pulse(dev);		\
			return ONE_WIRE_BUS_SIGNAL_RESET;		\
		} else if (v == ONE_WIRE_BUS_SIGNAL_TERMINATE) {	\
			dev->state = DS1963S_STATE_TERMINATED;		\
			return ONE_WIRE_BUS_SIGNAL_TERMINATE;		\
		}							\
									\
		v;							\
	})

#define DS1963S_TX_FAIL(dev)						\
	do {								\
		DS1963S_TX_BIT(dev, 1);					\
	} while (1)

#define DS1963S_TX_SUCCESS(dev)						\
	do {								\
		DS1963S_TX_BIT(dev, 0);					\
		DS1963S_TX_BIT(dev, 1);					\
	} while (1)

#define DS1963S_TX_END(dev)	DS1963S_TX_FAIL(dev)
#define DS1963S_RX_BIT(dev)	DS1963S_TX_BIT(dev, 1)
#define DS1963S_RX_BYTE(dev)	DS1963S_TX_BYTE(dev, 0xFF)

/* Calculate the value for MPX, as used in __sha1_get_input_1. */
static uint8_t
__mpx_get(int M, int X, uint8_t *SP)
{
	return (M << 7) | (X << 6) | (SP[12] & 0x3F);
}

/* Calculate the value for MP, as used in __sha1_get_input_2. */
static uint8_t
__mp_get(int M, int X, int page)
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
static void
__sha1_get_input_1(uint8_t M[64], uint8_t *SS, uint8_t *PP, uint8_t MPX,
                   uint8_t *SP)
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
static void
__sha1_get_input_2(uint8_t M[64], uint8_t *SS, uint8_t *CC, uint8_t *PP,
                   uint8_t FAMC, uint8_t MP, uint8_t *SN, uint8_t *SP)
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
static void
__sha1_get_output_1(uint8_t SP[32], uint32_t A, uint32_t B, uint32_t C,
                    uint32_t D, uint32_t E)
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

/* Construct the SHA-1 output for operations:
 *
 * - Compute First Secret
 * - Compute Next Secret
 */
static void
__sha1_get_output_2(uint8_t SP[32], uint32_t D, uint32_t E)
{
	for (int i = 0; i < 32; i += 8) {
		SP[i + 0] = (E >>  0) & 0xFF;
		SP[i + 1] = (E >>  8) & 0xFF;
		SP[i + 2] = (E >> 16) & 0xFF;
		SP[i + 3] = (E >> 24) & 0xFF;

		SP[i + 4] = (D >>  0) & 0xFF;
		SP[i + 5] = (D >>  8) & 0xFF;
		SP[i + 6] = (D >> 16) & 0xFF;
		SP[i + 7] = (D >> 24) & 0xFF;
	}
}

static inline void
__ds1963s_dev_do_reset_pulse(struct ds1963s_device *dev)
{
	DEBUG_LOG("[ds1963s] got reset pulse\n");
	dev->state = DS1963S_STATE_RESET;
}

void ds1963s_dev_init(struct ds1963s_device *ds1963s)
{
	memset(ds1963s, 0, sizeof *ds1963s);

	ds1963s->family = DS1963S_DEVICE_FAMILY;
	ds1963s->ES     = 0x1f;
	memset(ds1963s->data_memory,   0xaa, sizeof ds1963s->data_memory);
	memset(ds1963s->secret_memory, 0xaa, sizeof ds1963s->secret_memory);
	memset(ds1963s->scratchpad,    0xff, sizeof ds1963s->scratchpad);

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

int
ds1963s_dev_pf_get(struct ds1963s_device *dev)
{
	return (dev->ES >> 5) & 1;
}

void
ds1963s_dev_pf_set(struct ds1963s_device *dev, int pf)
{
	dev->ES &= ~(1 << 5);
	dev->ES |= pf << 5;
}

uint16_t
ds1963s_dev_address_get(struct ds1963s_device *dev)
{
	return ds1963s_ta_to_address(dev->TA1, dev->TA2) % 0x400;
}

int
ds1963s_dev_page_get(struct ds1963s_device *dev)
{
	return ds1963s_address_to_page(ds1963s_dev_address_get(dev));
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

	one_wire_bus_member_add(&ds1963s->bus_slave, bus);
}

void ds1963s_dev_erase_scratchpad(struct ds1963s_device *dev, int address)
{
	assert(dev != NULL);
	assert(address >= 0 && address <= 65536);

	dev->CHLG = 0;
	dev->AUTH = 0;
	dev->TA1  = address & 0xFF;
	dev->TA2  = (address >> 8) & 0xFF;
	dev->ES   = 0x1f;

	memset(dev->scratchpad, 0xFF, sizeof dev->scratchpad);

	dev->HIDE = 0;
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

static void
__ds1963s_dev_compute_secret(struct ds1963s_device *dev, uint8_t secret[8])
{
	uint8_t  M[64];
	uint16_t addr;
	int      page;
	uint8_t  *SS;
	SHA1_CTX ctx;

	assert(dev != NULL);

	SS         = secret;
	dev->ES   |= 0x1F;	/* E4:E0 := 1,1,1,1,1 */
	dev->M     = 0;
	dev->X     = 0;
	dev->HIDE  = 1;
	dev->CHLG  = 0;
	dev->AUTH  = 0;
	dev->MATCH = 0;

	/* XXX: unclear of the ds1963s page aligns all this or just uses
	 * the address.  Test later.
	 */
	addr = ds1963s_ta_to_address(dev->TA1, dev->TA2);
	page = ds1963s_address_to_page(addr);

	__sha1_get_input_1(
		M,
		SS,
		&dev->data_memory[(page % 16) * 32],
		__mpx_get(dev->M, dev->X, dev->scratchpad),
		dev->scratchpad
	);

	/* We omit the finalize, as the DS1963S does not use it, but rather
	 * uses the internal state A, B, C, D, E for the result.  This also
	 * means we have to subtract the initial state from the context, as
	 * it has added these to the results.
	 */
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, M, sizeof M);

	/* Write the results to the scratchpad. */
	__sha1_get_output_2(
		dev->scratchpad,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);

	dev->prng_counter++;
}

void
ds1963s_dev_compute_first_secret(struct ds1963s_device *dev)
{
	uint8_t secret[8] = { 0 };
	__ds1963s_dev_compute_secret(dev, secret);
}

void
ds1963s_dev_compute_next_secret(struct ds1963s_device *dev)
{
	int addr, page;

	/* Check the page this address belongs to. */
	addr = ds1963s_ta_to_address(dev->TA1, dev->TA2);
	page = ds1963s_address_to_page(addr);

	__ds1963s_dev_compute_secret(dev, &dev->secret_memory[(page % 8) * 8]);
}

void
ds1963s_dev_read_auth_page(struct ds1963s_device *dev, int page)
{
	uint8_t M[64];
	uint8_t CC[4];
	SHA1_CTX ctx;

	PUT_32BIT_LSB(CC, dev->data_wc[page]);

	__sha1_get_input_2(
		M,
		&dev->secret_memory[(page % 8) * 8],
		CC,
		&dev->data_memory[(page % 16) * 32],
		DS1963S_DEVICE_FAMILY,
		__mp_get(dev->M, dev->X, page),
		dev->serial,
		dev->scratchpad
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
		dev->scratchpad,
		ctx.state[0] - 0x67452301,
		ctx.state[1] - 0xEFCDAB89,
		ctx.state[2] - 0x98BADCFE,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);

	dev->prng_counter++;
}

static int
__ds1963s_dev_sign_data_page(struct ds1963s_device *dev)
{
	uint8_t  M[64];
	int      page;
	SHA1_CTX ctx;

	assert(dev != NULL);

	/* XXX: fix M handling later. */
	dev->M    = 0;

	dev->X    = 0;
	dev->CHLG = 0;
	dev->AUTH = 0;

	/* The specifications state T4:T0 = 00000b, but this is not actually
	 * set in the TA1 register, as can be verified with ds1963s-shell.
	 * We will align the address used down ourselves in the SHA input.
	 */
	page = ds1963s_dev_page_get(dev);

	__sha1_get_input_1(
		M,
		&dev->secret_memory[(page % 8) * 8],
		&dev->memory[page * 32],
		__mpx_get(dev->M, dev->X, dev->scratchpad),
		dev->scratchpad
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
		dev->scratchpad,
		ctx.state[0] - 0x67452301,
		ctx.state[1] - 0xEFCDAB89,
		ctx.state[2] - 0x98BADCFE,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);

	dev->prng_counter++;

	return 0;
}

int
ds1963s_dev_sign_data_page(struct ds1963s_device *dev)
{
	uint16_t addr;
	int      page;

	assert(dev != NULL);

	addr = ds1963s_ta_to_address(dev->TA1, dev->TA2);
	page = ds1963s_address_to_page(addr);

	if (page != 0 && page != 8)
		return -1;

	return __ds1963s_dev_sign_data_page(dev);
}

int
ds1963s_dev_compute_challenge(struct ds1963s_device *dev)
{
	uint8_t  M[64];
	uint8_t  CC[4];
	int      page;
	SHA1_CTX ctx;

	/* The specifications state T4:T0 = 00000b, but this is not actually
	 * set in the TA1 register, as can be verified with ds1963s-shell.
	 * We will align the address used down ourselves in the SHA input.
	 */
	dev->M     = 0;
	dev->X     = 1;
	dev->CHLG  = 1;
	dev->AUTH  = 0;
	dev->MATCH = 0;

	PUT_32BIT_LSB(CC, dev->prng_counter);
	page = ds1963s_dev_page_get(dev);

	__sha1_get_input_2(
		M,
		&dev->secret_memory[(page % 8) * 8],
		CC,
		&dev->data_memory[(page % 16) * 32],
		DS1963S_DEVICE_FAMILY,
		__mp_get(dev->M, dev->X, page),
		dev->serial,
		dev->scratchpad
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
		dev->scratchpad,
		ctx.state[0] - 0x67452301,
		ctx.state[1] - 0xEFCDAB89,
		ctx.state[2] - 0x98BADCFE,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);

	dev->prng_counter++;

	return 0;
}

int
ds1963s_dev_authenticate_host(struct ds1963s_device *dev)
{
	uint8_t  M[64];
	int      page;
	SHA1_CTX ctx;

	assert(dev != NULL);

	dev->M     = 0;
	dev->X     = 1;
	dev->HIDE  = 1;
	dev->MATCH = 0;

	page = ds1963s_dev_page_get(dev);

	__sha1_get_input_1(
		M,
		&dev->secret_memory[(page % 8) * 8],
		&dev->memory[page * 32],
		__mpx_get(dev->M, dev->X, dev->scratchpad),
		dev->scratchpad
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
		dev->scratchpad,
		ctx.state[0] - 0x67452301,
		ctx.state[1] - 0xEFCDAB89,
		ctx.state[2] - 0x98BADCFE,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);

	dev->prng_counter++;

	if (dev->CHLG == 1) {
		/* XXX: FIXME */
		dev->AUTH = 1;
		dev->CHLG = 0;
	}

	return 0;
}

int
ds1963s_dev_validate_data_page(struct ds1963s_device *dev)
{
	assert(dev != NULL);

	dev->HIDE = 1;
	return __ds1963s_dev_sign_data_page(dev);
}

void
ds1963s_dev_rom_command_resume(struct ds1963s_device *dev)
{
	if (dev->RC == 1)
		dev->state = DS1963S_STATE_MEMORY_FUNCTION;
	else
		dev->state = DS1963S_STATE_RESET_WAIT;
}

int
ds1963s_dev_rom_command_search_rom(struct ds1963s_device *dev)
{
	uint8_t rom_code[8];

	dev->RC = 0;
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
	dev->RC = 1;

	return 0;
}

int
ds1963s_dev_rom_function(struct ds1963s_device *dev)
{
	int byte;

	byte = DS1963S_RX_BYTE(dev);

	switch (byte) {
	case 0x33:
		DEBUG_LOG("[ds1963s|ROM] Read ROM Command\n");
		fprintf(stderr, "Read ROM Command is not supported\n");
		exit(EXIT_FAILURE);
	case 0x3C:
		DEBUG_LOG("[ds1963s|ROM] Overdrive skip ROM\n");
		dev->RC = 0;
		dev->OD = 1;
		break;
	case 0x55:
		DEBUG_LOG("[ds1963s|ROM] Match ROM Command\n");
		fprintf(stderr, "Match ROM Command is not supported\n");
		exit(EXIT_FAILURE);
	case 0x69:
		DEBUG_LOG("[ds1963s|ROM] Overdrive Match Command\n");
		fprintf(stderr, "Overdrive Match Command is not supported\n");
		exit(EXIT_FAILURE);
	case 0xA5:
		DEBUG_LOG("[ds1963s|ROM] Resume\n");
		ds1963s_dev_rom_command_resume(dev);
		break;
	case 0xCC:
		DEBUG_LOG("[ds1963s|ROM] Skip ROM Command\n");
		dev->RC = 0;
		break;
	case 0xF0:
		DEBUG_LOG("[ds1963s|ROM] Search ROM\n");
		if (ds1963s_dev_rom_command_search_rom(dev) == -1)
			return -1;

		dev->state = DS1963S_STATE_MEMORY_FUNCTION;
		break;
	default:
		DEBUG_LOG("[ds1963s|ROM] Unknown command %.2x\n", byte);
		break;
	}

	return 0;
}

int
ds1963s_dev_memory_command_write_scratchpad(struct ds1963s_device *dev)
{
	uint8_t  offset;
	uint16_t crc16;
	uint16_t addr;

	dev->CHLG = 0;
	dev->AUTH = 0;
	dev->TA1  = DS1963S_RX_BYTE(dev);
	dev->TA2  = DS1963S_RX_BYTE(dev);
	addr      = ds1963s_ta_to_address(dev->TA1, dev->TA2);
	offset    = dev->TA1 & 0x1F;

	crc16 = 0;
	crc16 = ds1963s_crc16_update_byte(crc16, 0x0F);
	crc16 = ds1963s_crc16_update_byte(crc16, dev->TA1);
	crc16 = ds1963s_crc16_update_byte(crc16, dev->TA2);

	if (dev->HIDE == 0) {
		if (addr >= 0x200) goto done;
		dev->ES &= 0x5F;		/* PF := 0; AA := 0 */
	} else {
		if (!ds1963s_address_secret(addr)) goto done;
		dev->ES  &= 0x5F;		/* PF := 0; AA := 0 */
		dev->TA1 &= 0xF8;		/* T2:T0 := 0,0,0 */
		dev->ES  &= 0xE0;		/* E4:E0 := 0,0,0,0,0 */
		dev->ES  |= dev->TA1 & 0x1F;	/* E4:E0 := T4:T0     */
		dev->ES  |= 0x07;		/* E2:E0 := 1,1,1     */
	}

	for (int i = 3; offset < 32; i++, offset++) {
		int byte;

		/* XXX: does not properly handle 8-bit boundaries. */
		byte = one_wire_bus_member_rx_byte(&dev->bus_slave);
		if (byte == ONE_WIRE_BUS_SIGNAL_RESET) {
			__ds1963s_dev_do_reset_pulse(dev);
			dev->PF = 1;
			return ONE_WIRE_BUS_SIGNAL_RESET;
		}

		if (dev->HIDE == 0)
			dev->scratchpad[offset] = byte;

		crc16 = ds1963s_crc16_update_byte(crc16, byte);
	}

	/* If the full scratchpad was written we send the crc16. */
	if (offset == 32) {
		crc16 = ~crc16;
		DS1963S_TX_BYTE(dev, crc16 & 0xFF);
		DS1963S_TX_BYTE(dev, crc16 >> 8);
	}

done:
	DS1963S_TX_END(dev);
}

int
ds1963s_dev_sha_command_compute_first_secret(struct ds1963s_device *dev)
{
	ds1963s_dev_compute_first_secret(dev);

	/* XXX: Investigate how many 1s to send later. */
	for (int i = 0; i < 10; i++)
		DS1963S_TX_BIT(dev, 1);

	DS1963S_TX_SUCCESS(dev);
}


int
ds1963s_dev_sha_command_compute_next_secret(struct ds1963s_device *dev)
{
	ds1963s_dev_compute_next_secret(dev);

	/* XXX: Investigate how many 1s to send later. */
	for (int i = 0; i < 10; i++)
		DS1963S_TX_BIT(dev, 1);

	DS1963S_TX_SUCCESS(dev);
}

int
ds1963s_dev_sha_command_validate_data_page(struct ds1963s_device *dev)
{
	if (ds1963s_dev_validate_data_page(dev) == -1)
		DS1963S_TX_FAIL(dev);

	/* XXX: Investigate how many 1s to send later. */
	for (int i = 0; i < 10; i++)
		DS1963S_TX_BIT(dev, 1);

	DS1963S_TX_SUCCESS(dev);
}

int
ds1963s_dev_sha_command_sign_data_page(struct ds1963s_device *dev)
{
	if (ds1963s_dev_sign_data_page(dev) == -1)
		DS1963S_TX_FAIL(dev);

	/* XXX: Investigate how many 1s to send later. */
	for (int i = 0; i < 10; i++)
		DS1963S_TX_BIT(dev, 1);

	DS1963S_TX_SUCCESS(dev);
}


int
ds1963s_dev_sha_command_compute_challenge(struct ds1963s_device *dev)
{
	if (ds1963s_dev_compute_challenge(dev) == -1)
		DS1963S_TX_FAIL(dev);

	/* XXX: Investigate how many 1s to send later. */
	for (int i = 0; i < 10; i++)
		DS1963S_TX_BIT(dev, 1);

	DS1963S_TX_SUCCESS(dev);
}

int
ds1963s_dev_sha_command_authenticate_host(struct ds1963s_device *dev)
{
	if (ds1963s_dev_authenticate_host(dev) == -1)
		DS1963S_TX_FAIL(dev);

	/* XXX: Investigate how many 1s to send later. */
	for (int i = 0; i < 10; i++)
		DS1963S_TX_BIT(dev, 1);

	DS1963S_TX_SUCCESS(dev);
}

int
ds1963s_dev_memory_command_compute_sha(struct ds1963s_device *dev)
{
	uint16_t crc16;
	uint8_t  ctrl;

	dev->TA1 = DS1963S_RX_BYTE(dev);
	dev->TA2 = DS1963S_RX_BYTE(dev);
	ctrl     = DS1963S_RX_BYTE(dev);

	crc16    = 0;
	crc16    = ds1963s_crc16_update_byte(crc16, 0x33);
	crc16    = ds1963s_crc16_update_byte(crc16, dev->TA1);
	crc16    = ds1963s_crc16_update_byte(crc16, dev->TA2);
	crc16    = ds1963s_crc16_update_byte(crc16, ctrl);
	crc16    = ~crc16;
	DS1963S_TX_BYTE(dev, crc16 & 0xFF);
	DS1963S_TX_BYTE(dev, crc16 >> 8);

	switch (ctrl) {
	case 0x0F:
		DEBUG_LOG("[ds1963s|SHA] Compute 1st Secret\n");
		ds1963s_dev_sha_command_compute_first_secret(dev);
		break;
	case 0xF0:
		DEBUG_LOG("[ds1963s|SHA] Compute next Secret\n");
		ds1963s_dev_sha_command_compute_next_secret(dev);
		break;
	case 0x3C:
		DEBUG_LOG("[ds1963s|SHA] Validate Data Page\n");
		ds1963s_dev_sha_command_validate_data_page(dev);
		break;
	case 0xC3:
		DEBUG_LOG("[ds1963s|SHA] Sign Data Page\n");
		return ds1963s_dev_sha_command_sign_data_page(dev);
	case 0xCC:
		DEBUG_LOG("[ds1963s|SHA] Compute Challenge\n");
		ds1963s_dev_sha_command_compute_challenge(dev);
		break;
	case 0xAA:
		DEBUG_LOG("[ds1963s|SHA] Authenticate Host\n");
		ds1963s_dev_sha_command_authenticate_host(dev);
		break;
	default:
		DEBUG_LOG("[ds1963s|SHA] Unknown command %.2x\n", ctrl);
		break;
	}

	return 0;
}

int
ds1963s_dev_memory_command_copy_scratchpad(struct ds1963s_device *dev)
{
	uint8_t  TA1, TA2, ES;
	uint16_t addr;

	dev->CHLG = 0;
	dev->AUTH = 0;

	TA1  = DS1963S_RX_BYTE(dev);
	TA2  = DS1963S_RX_BYTE(dev);
	ES   = DS1963S_RX_BYTE(dev);
	addr = ds1963s_ta_to_address(TA1, TA2);

	if (dev->HIDE == 0 && addr >= 0x200)
		goto error;

	if (dev->HIDE == 1 && !ds1963s_address_secret(addr))
		goto error;

	if (TA1 != dev->TA1 || TA2 != dev->TA2 || ES != dev->ES)
		goto error;

	dev->AA = 1;
	memcpy(&dev->memory[addr], dev->scratchpad, (ES & 0x1F) + 1);

	hexdump(dev->secret_memory, sizeof dev->secret_memory, 0);

	/* XXX: specs say this happens for 32us.  Investigate how many 1s
	 * to send later.
	 */
	for (int i = 0; i < 8; i++)
		DS1963S_TX_BIT(dev, 1);

	DS1963S_TX_SUCCESS(dev);

error:
	DS1963S_TX_FAIL(dev);
}

int
ds1963s_dev_memory_command_match_scratchpad(struct ds1963s_device *dev)
{
	uint8_t  buf[20];
	uint16_t crc16;

	dev->CHLG  = 0;
	dev->MATCH = 0;

	crc16 = 0;
	crc16 = ds1963s_crc16_update_byte(crc16, 0x3C);

	for (int i = 0; i < 20; i++) {
		buf[i] = DS1963S_RX_BYTE(dev);
		crc16  = ds1963s_crc16_update_byte(crc16, buf[i]);
	}

	/* XXX: datasheet seems to state this is independent of the comparison
	 * result.  Is this correct?  Need to verify.
	 */
	if (dev->AUTH) {
		dev->AUTH  = 0;
		dev->MATCH = 1;
	}

	crc16 = ~crc16;
	DS1963S_TX_BYTE(dev, crc16 & 0xFF);
	DS1963S_TX_BYTE(dev, crc16 >> 8);

	if (!memcmp(&dev->scratchpad[8], buf, 20))
		DS1963S_TX_SUCCESS(dev);

	DS1963S_TX_FAIL(dev);
}

int
ds1963s_dev_memory_command_read_authenticated_page(struct ds1963s_device *dev)
{
	uint8_t  TA1, TA2;
	uint8_t  CC[4];
	uint16_t crc16;
	uint16_t addr;
	int      page;

	dev->CHLG = 0;
	dev->AUTH = 0;

	TA1  = DS1963S_RX_BYTE(dev);
	TA2  = DS1963S_RX_BYTE(dev);
	addr = ds1963s_ta_to_address(TA1, TA2);
	page = ds1963s_address_to_page(addr);

	if (addr >= 0x200)
		goto error;

	crc16 = 0;
	crc16 = ds1963s_crc16_update_byte(crc16, 0xA5);
	crc16 = ds1963s_crc16_update_byte(crc16, TA1);
	crc16 = ds1963s_crc16_update_byte(crc16, TA2);

	do {
		DS1963S_TX_BYTE(dev, dev->memory[addr]);
		crc16 = ds1963s_crc16_update_byte(crc16, dev->memory[addr]);
	} while (++addr % 32 != 0);

	/* TX the write cycle counter of the page. */
	PUT_32BIT_LSB(CC, dev->data_wc[page]);
	for (int j = 0; j < 4; j++) {
		DS1963S_TX_BYTE(dev, CC[j]);
		crc16 = ds1963s_crc16_update_byte(crc16, CC[j]);
	}

	/* TX the write cycle counter of secret for this page. */
	PUT_32BIT_LSB(CC, dev->secret_wc[page]);
	for (int j = 0; j < 4; j++) {
		DS1963S_TX_BYTE(dev, CC[j]);
		crc16 = ds1963s_crc16_update_byte(crc16, CC[j]);
	}

	/* TX the crc16 of everything done so far. */
	crc16 = ~crc16;
	DS1963S_TX_BYTE(dev, crc16 & 0xFF);
	DS1963S_TX_BYTE(dev, crc16 >> 8);

	dev->X = 0;
	/* XXX: FIXME AND WORK OUT THE LOGIC FOR M. */
	dev->M = 0;

	/* Construct the SHA-1 hash on the scratchpad. */
	ds1963s_dev_read_auth_page(dev, page);

	/* XXX: Investigate how many 1s to send later. */
	for (int i = 0; i < 10; i++)
		DS1963S_TX_BIT(dev, 1);

	DS1963S_TX_SUCCESS(dev);

error:
	DS1963S_TX_FAIL(dev);
}

int
ds1963s_dev_memory_command_read_scratchpad(struct ds1963s_device *dev)
{
	uint8_t  offset;
	uint16_t crc16;
	int      i;

	DEBUG_LOG("TA1: %x TA2: %x\n", dev->TA1, dev->TA2);

	DS1963S_TX_BYTE(dev, dev->TA1);
	DS1963S_TX_BYTE(dev, dev->TA2);
	DS1963S_TX_BYTE(dev, dev->ES);

	offset = dev->TA1 & 0x1F;

	crc16 = 0;
	crc16 = ds1963s_crc16_update_byte(crc16, 0xAA);
	crc16 = ds1963s_crc16_update_byte(crc16, dev->TA1);
	crc16 = ds1963s_crc16_update_byte(crc16, dev->TA2);
	crc16 = ds1963s_crc16_update_byte(crc16, dev->ES);

	for (i = 4; offset < 32; i++, offset++) {
		int byte = dev->HIDE ? 0xFF : dev->scratchpad[offset];
		DS1963S_TX_BYTE(dev, byte);
		crc16 = ds1963s_crc16_update_byte(crc16, byte);
	}

	crc16 = ~crc16;
	DS1963S_TX_BYTE(dev, crc16 & 0xFF);
	DS1963S_TX_BYTE(dev, crc16 >> 8);
	DS1963S_TX_END(dev);
}

int
ds1963s_dev_memory_command_erase_scratchpad(struct ds1963s_device *dev)
{
	dev->CHLG = 0;
	dev->AUTH = 0;
	dev->TA1  = DS1963S_RX_BYTE(dev);
	dev->TA2  = DS1963S_RX_BYTE(dev);
	dev->ES   = 0x1f;

	memset(dev->scratchpad, 0xFF, sizeof dev->scratchpad);
	/* Simulate TX 1s until the command is finished.
	 *
	 * XXX: specs say this happens for 32us.  Investigate how many 1s
	 * to send later.
	 */
	for (int i = 0; i < 10; i++)
		DS1963S_TX_BIT(dev, 1);

	dev->HIDE = 0;

	DS1963S_TX_SUCCESS(dev);
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

	DS1963S_TX_END(dev);
}

int
ds1963s_dev_memory_function(struct ds1963s_device *dev)
{
	int byte;

	assert(dev != NULL);

	byte = DS1963S_RX_BYTE(dev);

	switch (byte) {
	case 0x0F:
		DEBUG_LOG("[ds1963s|MEMORY] Write Scratchpad\n");
		ds1963s_dev_memory_command_write_scratchpad(dev);
		break;
	case 0x33:
		DEBUG_LOG("[ds1963s|MEMORY] Compute SHA\n");
		ds1963s_dev_memory_command_compute_sha(dev);
		break;
	case 0x3C:
		DEBUG_LOG("[ds1963s|MEMORY] Match Scratchpad\n");
		ds1963s_dev_memory_command_match_scratchpad(dev);
		break;
	case 0x55:
		DEBUG_LOG("[ds1963s|MEMORY] Copy Scratchpad\n");
		ds1963s_dev_memory_command_copy_scratchpad(dev);
		break;
	case 0xA5:
		DEBUG_LOG("[ds1963s|MEMORY] Read Authenticated Page\n");
		ds1963s_dev_memory_command_read_authenticated_page(dev);
		break;
	case 0xAA:
		DEBUG_LOG("[ds1963s|MEMORY] Read Scratchpad\n");
		ds1963s_dev_memory_command_read_scratchpad(dev);
		break;
	case 0xC3:
		DEBUG_LOG("[ds1963s|MEMORY] Erase Scratchpad\n");
		ds1963s_dev_memory_command_erase_scratchpad(dev);
		break;
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

	while (dev->state != DS1963S_STATE_TERMINATED) {
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
			DEBUG_LOG("[ds1963s|RESET] Waiting for ROM function...\n");

#if 0			/* XXX: hack, we don't hot-add to the topology anyway,
			 * so for now we don't need this.
			 */
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
