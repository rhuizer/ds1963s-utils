/* ds1963s-client.h
 *
 * A ds1963s emulation and utility framework.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * Copyright (C) 2013-2019  Ronald Huizer <rhuizer@hexpedition.com>
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
#ifndef DS1963S_CLIENT_H
#define DS1963S_CLIENT_H

#include <stdint.h>
#include "ibutton/shaib.h"
#include "ds1963s-error.h"

#define DS1963S_HASH_SIZE		20
#define DS1963S_PAGE_SIZE		32
#define DS1963S_SCRATCHPAD_SIZE		32
#define DS1963S_SERIAL_SIZE		6
#define DS1963S_ROM_COMMAND_READ	0x33

#define WRITE_CYCLE_DATA_8	0
#define WRITE_CYCLE_DATA_9	1
#define WRITE_CYCLE_DATA_10	2
#define WRITE_CYCLE_DATA_11	3
#define WRITE_CYCLE_DATA_12	4
#define WRITE_CYCLE_DATA_13	5
#define WRITE_CYCLE_DATA_14	6
#define WRITE_CYCLE_DATA_15	7
#define WRITE_CYCLE_SECRET_0	8
#define WRITE_CYCLE_SECRET_1	9
#define WRITE_CYCLE_SECRET_2	10
#define WRITE_CYCLE_SECRET_3	11
#define WRITE_CYCLE_SECRET_4	12
#define WRITE_CYCLE_SECRET_5	13
#define WRITE_CYCLE_SECRET_6	14
#define WRITE_CYCLE_SECRET_7	15

typedef struct ds1963s_client
{
	const char	*device_path;
	SHACopr		copr;
	int		resume;
	int		errno;
} ds1963s_client_t;

typedef struct
{
	uint8_t		data[DS1963S_SCRATCHPAD_SIZE];
	uint8_t		data_size;
	uint16_t	address;
	uint8_t		es;
	uint16_t	crc16;
	int		crc_ok;
} ds1963s_client_sp_read_reply_t;

typedef struct
{
	uint8_t		data[32];
	uint8_t		data_size;
	uint32_t	data_wc;
	uint32_t	secret_wc;
	uint16_t	crc16;
	int		crc_ok;
} ds1963s_client_read_auth_page_reply_t;

typedef struct ds1963s_rom {
	uint8_t		raw[8];
	uint8_t		family;
	uint8_t		serial[6];
	uint8_t		crc;
	int		crc_ok;
} ds1963s_rom_t;

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

int  ds1963s_client_init(struct ds1963s_client *ctx, const char *device);
void ds1963s_client_destroy(struct ds1963s_client *ctx);
int  ds1963s_client_page_to_address(struct ds1963s_client *ctx, int page);
int  ds1963s_client_address_to_page(struct ds1963s_client *ctx, int address);

/* Scratchpad related functions. */
int ds1963s_client_sp_copy(ds1963s_client_t *, int address, uint8_t es);
int ds1963s_client_sp_erase(ds1963s_client_t *, int address);
int ds1963s_client_sp_match(ds1963s_client_t *, uint8_t hash[20]);
int ds1963s_client_sp_read(ds1963s_client_t *, ds1963s_client_sp_read_reply_t *);
int ds1963s_client_sp_write(ds1963s_client_t *, uint16_t address, const uint8_t *data, size_t len);

int ds1963s_client_read_auth(ds1963s_client_t *, int, ds1963s_client_read_auth_page_reply_t *);

/* Compute SHA commands. */
int ds1963s_client_sha_command(ds1963s_client_t *ctx, uint8_t cmd, int address);
int ds1963s_client_secret_compute_first(ds1963s_client_t *ctx, int address);
int ds1963s_client_secret_compute_next(ds1963s_client_t *ctx, int address);
int ds1963s_client_validate_data_page(ds1963s_client_t *ctx, int address);
int ds1963s_client_sign_data_page(ds1963s_client_t *ctx, int address);
int ds1963s_client_compute_challenge(ds1963s_client_t *ctx, int address);
int ds1963s_client_authenticate_host(ds1963s_client_t *ctx, int address);

void ds1963s_client_rom_get(ds1963s_client_t *, ds1963s_rom_t *);
int  ds1963s_client_taes_get(struct ds1963s_client *ctx, uint16_t *addr, uint8_t *es);
int  ds1963s_client_taes_print(struct ds1963s_client *ctx);
void ds1963s_client_hash_print(uint8_t hash[20]);

int ds1963s_client_memory_read(struct ds1963s_client *ctx, uint16_t address,
                               uint8_t *data, size_t size);
int ds1963s_client_memory_write(struct ds1963s_client *ctx, uint16_t address,
                                const uint8_t *data, size_t size);
uint32_t ds1963s_client_prng_get(struct ds1963s_client *ctx);

void ds1963s_client_perror(struct ds1963s_client *ctx, const char *s, ...);
int ds1963s_write_cycle_get_all(struct ds1963s_client*, uint32_t [16]);
int ds1963s_client_hide_set(struct ds1963s_client *ctx);

int ds1963s_client_secret_write(struct ds1963s_client *ctx, int secret,
                                const void *data, size_t len);
int ds1963s_client_hash_read(struct ds1963s_client *ctx, uint8_t hash[20]);

#ifdef __cplusplus
};
#endif	/* __cplusplus */

#endif	/* !DS1963S_CLIENT_H */
