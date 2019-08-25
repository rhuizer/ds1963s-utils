/* ds1963s-client.c
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
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "getput.h"
#include "ibutton/ds2480.h"
#include "ibutton/ownet.h"
#include "ibutton/shaib.h"
#include "ds1963s-common.h"
#include "ds1963s-client.h"
#include "ds1963s-error.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

extern int fd[MAX_PORTNUM];
void owClearError(void);

static int __ds1963s_acquire(struct ds1963s_client *ctx, const char *port)
{
	int portnum;

	if ( (portnum = OpenCOMEx(port)) < 0) {
		ctx->errno = DS1963S_ERROR_OPENCOM;
		return -1;
	}

	if (!DS2480Detect(portnum)) {
		CloseCOM(portnum);
		ctx->errno = DS1963S_ERROR_NO_DS2480;
		return -1;
	}

	return portnum;
}

static int
__ds1963s_find(struct ds1963s_client *ctx, int portnum, unsigned char *devAN)
{
	/* XXX: FindNewSHA can return FALSE without adding an error to the
	 * error stack (when it does not find any buttons).  We need to
	 * differentiate.
	 */
	owClearError();

	if (FindNewSHA(portnum, devAN, TRUE) == TRUE)
		return 0;

	/* XXX: this is dependent on FindNewSHA internals. */
	if (owHasErrors()) {
		ctx->errno = DS1963S_ERROR_SET_LEVEL;
		return -1;
	}

	/* We did not find any new SHA based ibuttons. */
	ctx->errno = DS1963S_ERROR_NOT_FOUND;
	return -1;
}

int
ds1963s_client_init(ds1963s_client_t *ctx, const char *device)
{
	SHACopr *copr = &ctx->copr;

	/* Get port. */
	if ( (copr->portnum = __ds1963s_acquire(ctx, device)) == -1)
		return -1;

	/* Find DS1963S iButton. */
	if (__ds1963s_find(ctx, copr->portnum, copr->devAN) == -1)
		return -1;

	ctx->resume      = 0;
	ctx->device_path = device;
	ctx->errno       = 0;

	return 0;
}

void
ds1963s_client_destroy(ds1963s_client_t *ctx)
{
	owRelease(ctx->copr.portnum);
}

static inline int
__ds1963s_client_select_sha(ds1963s_client_t *ctx)
{
	if (SelectSHA(ctx->copr.portnum) == FALSE) {
		ctx->errno = DS1963S_ERROR_ACCESS;
		return -1;
        }

	return 0;
}

int
ds1963s_client_page_to_address(ds1963s_client_t *ctx, int page)
{
	if (page < 0 || page > 21) {
		ctx->errno = DS1963S_ERROR_INVALID_PAGE;
		return -1;
	}

	return page * 32;
}

int
ds1963s_client_address_to_page(ds1963s_client_t *ctx, int address)
{
	if (address < 0 || address > 0x2c0) {
		ctx->errno = DS1963S_ERROR_INVALID_ADDRESS;
		return -1;
	}

	return address / 32;
}

void
ds1963s_client_rom_get(ds1963s_client_t *ctx, struct ds1963s_rom *rom)
{
	int portnum = ctx->copr.portnum;

	owSerialNum(portnum, rom->raw, TRUE);

	rom->family = rom->raw[0];
	rom->crc    = rom->raw[7];
	rom->crc_ok = ds1963s_crc8(rom->raw, 8) == 0;

	for (int i = 0; i < 6; i++)
		rom->serial[i] = rom->raw[6 - i];
}

int ds1963s_client_taes_print(struct ds1963s_client *ctx)
{
	uint16_t addr;
	uint8_t es;

	if (ds1963s_client_taes_get(ctx, &addr, &es) == -1)
		return -1;

	printf("TA2: 0x%.2x TA1: 0x%.2x E/S: 0x%.2x\n",
		addr >> 8, addr & 0xff, es);

	return 0;
}

void ds1963s_client_hash_print(uint8_t hash[20])
{
	int i;

	for (i = 0; i < 20; i++)
		printf("%.2x", hash[i]);
	printf("\n");
}

int
ds1963s_client_taes_get(struct ds1963s_client *ctx, uint16_t *addr, uint8_t *es)
{
	uint8_t buf[DS1963S_SCRATCHPAD_SIZE];
	int portnum = ctx->copr.portnum;
	uint8_t __es;
	int __addr;

        OWASSERT(ReadScratchpadSHA18(portnum, &__addr, &__es, buf, 0),
                 OWERROR_READ_SCRATCHPAD_FAILED, -1);

	if (addr)
		*addr = __addr;

	if (es)
		*es = __es;

	return 0;
}

int
ds1963s_client_hash_read(ds1963s_client_t *ctx, uint8_t h[DS1963S_HASH_SIZE])
{
	ds1963s_client_sp_read_reply_t reply;

	if (ds1963s_client_sp_read(ctx, &reply) == -1)
		return -1;

	if (reply.crc_ok == 0)
		return -1;

	if (reply.data_size != 32) {
		ctx->errno = DS1963S_ERROR_DATA_LEN;
		return -1;
	}

	memcpy(h, &reply.data[8], DS1963S_HASH_SIZE);

	return 0;
}

int
ds1963s_client_sp_read(ds1963s_client_t *ctx, ds1963s_client_sp_read_reply_t *reply)
{
	int portnum = ctx->copr.portnum;
	size_t bytes_read;
	uint8_t buf[40];
	uint16_t crc;
	int i = 0;

	if (ctx->resume)
		buf[i++] = ROM_CMD_RESUME;
	else if (__ds1963s_client_select_sha(ctx) == -1)
		return -1;

	buf[i++] = CMD_READ_SCRATCHPAD;

	/* Padding for transmission of TA1 TA2 E/S CRC16 and data. */
	memset(&buf[i], 0xff, 37);
	i += 37;

	/* Send the buffer out. */
	OWASSERT(owBlock(portnum, ctx->resume, buf, i), OWERROR_BLOCK_FAILED, -1);

	/* Calculate the bytes read from the scratchpad based on TA1(4:0). */
	bytes_read = buf[ctx->resume + 1];
	bytes_read = 32 - (bytes_read & 0x1F);

	/* Calculate the CRC16. */
	crc = ds1963s_crc16(&buf[ctx->resume], bytes_read + 6);

	/* Copy the data we've read. */
	reply->data_size = bytes_read;
	memcpy(reply->data, &buf[ctx->resume + 4], reply->data_size);

	reply->address = ds1963s_ta_to_address(buf[ctx->resume + 1],
	                                       buf[ctx->resume + 2]);

	reply->es     = buf[ctx->resume + 3];
	reply->crc16  = ~GET_16BIT_MSB(&buf[bytes_read + 4 + ctx->resume]);
	reply->crc_ok = crc == 0xB001;

	/* This is not a hard error because the client may be interested in
	 * the reply regardless of the checksum, but we flag the error here
	 * in case the client handles it.
	 */
	if (reply->crc_ok == 0)
		ctx->errno = DS1963S_ERROR_INTEGRITY;

	/* Return the number of bytes read. */
	return bytes_read;
}

int
ds1963s_client_read_auth(ds1963s_client_t *ctx, int address,
                         ds1963s_client_read_auth_page_reply_t *reply)
{
	int portnum = ctx->copr.portnum;
	uint8_t read_size;
	uint8_t buf[56];
	uint16_t crc;
	int num_verf;
	int i = 0;

	assert(ctx != NULL);
	assert(reply != NULL);
	assert(address >= 0 && address <= 0xFFFF);

	if (ctx->resume)
		buf[i++] = ROM_CMD_RESUME;
	else if (__ds1963s_client_select_sha(ctx) == -1)
		return -1;

	read_size = 32 - (address % 32);

	/* XXX: study how overdrive works. */
	num_verf = (in_overdrive[portnum&0x0FF]) ? 10 : 2;

	buf[i++] = CMD_READ_AUTH_PAGE;
	buf[i++] = address & 0xFF;
	buf[i++] = address >> 8;

	/* Padding for transmission of WC counters, TA2, CRC16, data, and
	 * the verification pattern.
	 */
	memset(&buf[i], 0xFF, 10 + read_size + num_verf);
	i += 10 + read_size + num_verf;

	/* Send the block. */
	OWASSERT(owBlock(portnum, ctx->resume, buf, i),
	         OWERROR_BLOCK_FAILED, -1);

	/* Calculate the CRC over the received data; that is command byte,
	 * address, data, counters, and the 16-bit crc received.
	 */
	crc = ds1963s_crc16(buf + ctx->resume, read_size + 13);

	/* The DS1963S sends 1 bits during SHA1 computation and signals
	 * that the SHA1 computation finished by sending an alternating pattern
	 * of 0 and 1 bits.  We detect this pattern here.
	 */
	OWASSERT( ((buf[i - 1] & 0xF0) == 0x50) ||
             ((buf[i - 1] & 0xF0) == 0xA0),
             OWERROR_NO_COMPLETION_BYTE, -1);

	memcpy(reply->data, &buf[i - 10 - read_size - num_verf], read_size);
	reply->data_size = read_size;
	reply->data_wc   = GET_32BIT_LSB(&buf[i - 10 - num_verf]);
	reply->secret_wc = GET_32BIT_LSB(&buf[i - 6 - num_verf]);
	reply->crc16     = ~GET_16BIT_MSB(&buf[10 + read_size + 1 + ctx->resume]);
	reply->crc_ok    = crc == 0xB001;

	return 0;
}

int
ds1963s_client_sha_command(ds1963s_client_t *ctx, uint8_t cmd, int address)
{
	int portnum;

	assert(ctx != NULL);
	assert(address >= 0 && address <= 0xFFFF);

       	portnum = ctx->copr.portnum;
	/* Generate the secret using the SHA cmd command. */
	if (SHAFunction18(portnum, cmd, address, ctx->resume) == FALSE) {
		ctx->errno = DS1963S_ERROR_SHA_FUNCTION;
		return -1;
	}

	return 0;
}

int
ds1963s_client_secret_compute_first(ds1963s_client_t *ctx, int address)
{
	return ds1963s_client_sha_command(ctx, 0x0F, address);
}

int
ds1963s_client_secret_compute_next(ds1963s_client_t *ctx, int address)
{
	return ds1963s_client_sha_command(ctx, 0xF0, address);
}

int
ds1963s_client_validate_data_page(ds1963s_client_t *ctx, int address)
{
	return ds1963s_client_sha_command(ctx, 0x3C, address);
}

int
ds1963s_client_sign_data_page(ds1963s_client_t *ctx, int address)
{
	return ds1963s_client_sha_command(ctx, 0xC3, address);
}

int
ds1963s_client_compute_challenge(ds1963s_client_t *ctx, int address)
{
	return ds1963s_client_sha_command(ctx, 0xCC, address);
}

int
ds1963s_client_authenticate_host(ds1963s_client_t *ctx, int address)
{
	return ds1963s_client_sha_command(ctx, 0xAA, address);
}

int
ds1963s_client_sp_copy(struct ds1963s_client *ctx, int address, uint8_t es)
{
	int     portnum = ctx->copr.portnum;
	uint8_t buf[10];
	int     num_verf;
	int     i = 0;

	if (ctx->resume) {
		buf[i++] = ROM_CMD_RESUME;
	} else if (__ds1963s_client_select_sha(ctx) == -1) {
		return -1;
	}

	// change number of verification bytes if in overdrive
	num_verf = (in_overdrive[portnum&0xFF]) ? 4 : 2;

	buf[i++] = CMD_COPY_SCRATCHPAD;
	buf[i++] = address & 0xFF;
	buf[i++] = (address >> 8) & 0xFF;
	buf[i++] = es;

	// verification bytes
	memset(&buf[i], 0xFF, num_verf);
	i += num_verf;

	// now send the block
	OWASSERT( owBlock(portnum, ctx->resume, buf, i),
		OWERROR_BLOCK_FAILED, FALSE );

	// check verification
	OWASSERT( ((buf[i - 1] & 0xF0) == 0x50) ||
	          ((buf[i - 1] & 0xF0) == 0xA0),
	         OWERROR_NO_COMPLETION_BYTE, FALSE);

	return 0;
}

int
ds1963s_client_sp_erase(struct ds1963s_client *ctx, int address)
{
	int portnum = ctx->copr.portnum;

	/* Erase the scratchpad to clear the HIDE flag. */
	if (EraseScratchpadSHA18(portnum, address, ctx->resume) == FALSE) {
		ctx->errno = DS1963S_ERROR_SP_ERASE;
		return -1;
	}

	return 0;
}

int
ds1963s_client_sp_match(struct ds1963s_client *ctx, uint8_t hash[20])
{
	int ret;

	assert(ctx != NULL);

	ret = MatchScratchpadSHA18(ctx->copr.portnum, hash, ctx->resume);
	if (ret == -1) {
		ctx->errno = DS1963S_ERROR_MATCH_SCRATCHPAD;
		return -1;
	}

	return ret == TRUE;
}

int
ds1963s_client_sp_write(struct ds1963s_client *ctx, uint16_t address,
                        const uint8_t *data, size_t len)
{
	int portnum = ctx->copr.portnum;
	uint8_t buf[64];
	int i = 0;

	/* Make sure the data we provide fits. */
	if (len > 32) {
		ctx->errno = DS1963S_ERROR_DATA_LEN;
		return -1;
	}

	if (ctx->resume) {
		buf[i++] = ROM_CMD_RESUME;
	} else if (__ds1963s_client_select_sha(ctx) == -1)
		return -1;

	/* write scratchpad command */
	buf[i++] = CMD_WRITE_SCRATCHPAD;
	/* TA1, which fully holds the offset. */
	buf[i++] = address & 0xFF;
	/* TA2, which is unused. */
	buf[i++] = address >> 8;
	/* payload */
	memcpy(buf + i, data, len);

	if (owBlock(portnum, 0, buf, len + i) == FALSE) {
		ctx->errno = DS1963S_ERROR_TX_BLOCK;
		return -1;
	}

	owTouchReset(portnum);

	return 0;
}

int ds1963s_client_memory_read(struct ds1963s_client *ctx, uint16_t address,
                               uint8_t *data, size_t size)
{
	int     portnum = ctx->copr.portnum;
	uint8_t block[160];

	if (size > sizeof(block) - 3) {
		ctx->errno = DS1963S_ERROR_DATA_LEN;
		return -1;
	}

	if (__ds1963s_client_select_sha(ctx) == -1)
		return -1;

	memset(block, 0xff, sizeof block);
	/* write scratchpad command */
	block[0] = CMD_READ_MEMORY;
	/* TA1, which fully holds the offset. */
	block[1] = address & 0xFF;
	/* TA2, which is unused. */
	block[2] = address >> 8;

	OWASSERT(owBlock(portnum, 0, block, size + 3), OWERROR_BLOCK_FAILED, -1);
	owTouchReset(portnum);

	memcpy(data, &block[3], size);
	return 0;
}

int ds1963s_client_memory_write(struct ds1963s_client *ctx, uint16_t address,
                                const uint8_t *data, size_t size)
{
	int portnum = ctx->copr.portnum;
	uint8_t buffer[32];
	uint8_t es = 0;
	int addr_buff;
	int ret;

	if (size > 32) {
		ctx->errno = DS1963S_ERROR_DATA_LEN;
		return -1;
	}

	/* Erase the scratchpad to clear the HIDE flag. */
	if (ds1963s_client_sp_erase(ctx, 0) == -1)
		return -1;

	/* Write the provided data to the scratchpad.  This will also latch in
 	 * TA1 and TA2, which are validated by the copy scratchpad command.
 	 */
	ret = ds1963s_client_sp_write(ctx, address, data, size);
	if (ret == -1)
		return -1;

	/* Read back the data from the scratchpad to verify TA/ES/data. */
	OWASSERT(ReadScratchpadSHA18(portnum, &addr_buff, &es, buffer, TRUE),
	         OWERROR_READ_SCRATCHPAD_FAILED, -1);

	/* Verify that what we read is exactly what we wrote. */
	OWASSERT(address == addr_buff, OWERROR_READ_SCRATCHPAD_FAILED, -1);
	OWASSERT(es + 1 == size, OWERROR_READ_SCRATCHPAD_FAILED, -1);
	OWASSERT(memcmp(buffer, data, size) == 0,
	         OWERROR_READ_SCRATCHPAD_FAILED, -1);

	/* We latched the data to scratchpad properly, copy to memory. */
	OWASSERT(CopyScratchpadSHA18(portnum, address, size, TRUE),
	         OWERROR_COPY_SCRATCHPAD_FAILED, -1);

	return 0;
}

static inline int
__write_cycle_address(int write_cycle_type)
{
	assert(write_cycle_type >= WRITE_CYCLE_DATA_8);
	assert(write_cycle_type <= WRITE_CYCLE_SECRET_7);

	return 0x260 + write_cycle_type * 4;
}

uint32_t ds1963s_client_write_cycle_get(struct ds1963s_client *ctx, int write_cycle_type)
{
	int address = __write_cycle_address(write_cycle_type);
	int portnum = ctx->copr.portnum;
	uint8_t block[7];

	if (__ds1963s_client_select_sha(ctx) == -1)
		return (uint32_t)-1;

	memset(block, 0xff, 7);
	/* write scratchpad command */
	block[0] = CMD_READ_MEMORY;
	/* TA1, which fully holds the offset. */
	block[1] = address & 0xFF;
	/* TA2, which is unused. */
	block[2] = address >> 8;

	OWASSERT(owBlock(portnum, 0, block, 7), OWERROR_BLOCK_FAILED, FALSE);
	owTouchReset(portnum);

	return GET_32BIT_LSB(&block[3]);
}

int
ds1963s_write_cycle_get_all(struct ds1963s_client *ctx, uint32_t counters[16])
{
	int address = __write_cycle_address(WRITE_CYCLE_DATA_8);
	int portnum = ctx->copr.portnum;
	uint8_t block[67];
	int i;

	if (__ds1963s_client_select_sha(ctx) == -1)
		return -1;

	memset(block, 0xff, sizeof block);
	/* write scratchpad command */
	block[0] = CMD_READ_MEMORY;
	/* TA1, which fully holds the offset. */
	block[1] = address & 0xFF;
	/* TA2, which is unused. */
	block[2] = address >> 8;

	OWASSERT(owBlock(portnum, 0, block, sizeof block), OWERROR_BLOCK_FAILED, -1);
	owTouchReset(portnum);

	for (i = 0; i < 16; i++)
		counters[i] = GET_32BIT_LSB(&block[i * 4 + 3]);

	return 0;
}

uint32_t ds1963s_client_prng_get(struct ds1963s_client *ctx)
{
	int portnum = ctx->copr.portnum;
	int address = 0x2A0;
	uint8_t block[7];

	if (__ds1963s_client_select_sha(ctx) == -1)
		return (uint32_t)-1;

	memset(block, 0xff, sizeof block);
	/* write scratchpad command */
	block[0] = CMD_READ_MEMORY;
	/* TA1, which fully holds the offset. */
	block[1] = address & 0xFF;
	/* TA2, which is unused. */
	block[2] = address >> 8;

	OWASSERT(owBlock(portnum, 0, block, sizeof block), OWERROR_BLOCK_FAILED, -1);
	owTouchReset(portnum);

	return GET_32BIT_LSB(&block[3]);
}

/* Pulling down the RTS and DTR lines on the serial port for a certain
 * amount of time power-on-resets the iButton.
 */
int ds1963s_client_hide_set(struct ds1963s_client *ctx)
{
	int status = 0;

	if (ioctl(fd[ctx->copr.portnum], TIOCMSET, &status) == -1) {
		ctx->errno = DS1963S_ERROR_SET_CONTROL_BITS;
		return -1;
	}

	/* XXX: unclear how long we should sleep for a power-on-reset. */
	sleep(1);

	/* Release and reacquire the port. */
	owRelease(ctx->copr.portnum);

        ctx->copr.portnum = __ds1963s_acquire(ctx, ctx->device_path);
	if (ctx->copr.portnum == -1)
		return -1;

	/* Find the DS1963S iButton again, as we've lost it after a
	 * return to probe condition.
	 */
	if (__ds1963s_find(ctx, ctx->copr.portnum, ctx->copr.devAN) == -1)
		return -1;

	return 0;
}

int ds1963s_client_secret_write(struct ds1963s_client *ctx, int secret,
                                const void *data, size_t len)
{
	SHACopr *copr = &ctx->copr;
	uint8_t buf[32];
	int secret_addr;
	int address;
	uint8_t es;

	if (secret < 0 || secret > 7) {
		ctx->errno = DS1963S_ERROR_SECRET_NUM;
		return -1;
	}

	if (len > 32) {
		ctx->errno = DS1963S_ERROR_SECRET_LEN;
		return -1;
	}

        secret_addr = 0x200 + secret * 8;

	/* Erase the scratchpad to clear the HIDE flag. */
	if (ds1963s_client_sp_erase(ctx, 0) == -1)
		return -1;

	/* Write the secret data to the scratchpad. */ 
	if (ds1963s_client_sp_write(ctx, 0, data, len) == -1)
		return -1;

	/* Read it back to validate it. */
	if (ReadScratchpadSHA18(copr->portnum, &address, &es, buf, 0) == FALSE) {
		ctx->errno = DS1963S_ERROR_SP_READ;
		return -1;
	}

	/* Verify if we read what we wrote out. */
	if (memcmp(buf, data, len) != 0) {
		ctx->errno = DS1963S_ERROR_INTEGRITY;
		return -1;
	}

	/* Now latch in TA1 and TA2 with secret_addr. */
	if (ds1963s_client_sp_write(ctx, secret_addr, data, len) == -1)
		return -1;

	/* Read back address and es for validation. */
	if (ReadScratchpadSHA18(copr->portnum, &address, &es, buf, 0) == FALSE) {
		ctx->errno = DS1963S_ERROR_SP_READ;
		return -1;
	}

	/* Set the hide flag so we can copy to the secret. */
	if (ds1963s_client_hide_set(ctx) == -1)
		return -1;

	/* ??? */
	if (ReadScratchpadSHA18(copr->portnum, &address, &es, buf, 0) == FALSE) {
		ctx->errno = DS1963S_ERROR_SP_READ;
		return -1;
	}

	/* Copy scratchpad data to the secret. */
	if (CopyScratchpadSHA18(copr->portnum, secret_addr, len, 0) == FALSE) {
		ctx->errno = DS1963S_ERROR_SP_COPY;
		return -1;
	}

	return 0;
}

void
ds1963s_client_perror(struct ds1963s_client *ctx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ds1963s_vperror(ctx->errno, fmt, ap);
	va_end(ap);
}
