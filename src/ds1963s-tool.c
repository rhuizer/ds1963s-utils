/* ds1963s-tool.c
 *
 * A utility for accessing the DS1963S iButton over 1-wire RS-232.
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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include "ds1963s-tool.h"
#include "ds1963s-common.h"
#ifdef HAVE_LIBYAML
#include "ds1963s-tool-yaml.h"
#endif

#define DEFAULT_SERIAL_PORT		"/dev/ttyUSB0"
#define MODE_INFO			1
#define MODE_READ			2
#define MODE_READ_AUTH			4
#define MODE_WRITE			8
#define MODE_SIGN			16
#define MODE_WRITE_SECRET		32
#define MODE_INFO_FULL			64
#define MODE_SECRET_FIRST_SET		128
#define MODE_SECRET_NEXT_SET		256
#define MODE_VALIDATE_DATA_PAGE		512

#define FORMAT_TEXT			1
#define FORMAT_YAML			2

int
ds1963s_tool_init(struct ds1963s_tool *tool, const char *device)
{
	memset(tool, 0, sizeof *tool);
	ds1963s_dev_init(&tool->brute.dev);
	return ds1963s_client_init(&tool->client, device);
}

void
ds1963s_tool_destroy(struct ds1963s_tool *tool)
{
	ds1963s_dev_destroy(&tool->brute.dev);
	ds1963s_client_destroy(&tool->client);
}

void
ds1963s_tool_fatal(struct ds1963s_tool *tool)
{
	ds1963s_tool_destroy(tool);
	exit(EXIT_FAILURE);
}

int
ds1963s_tool_secret_hmac_target_get(
	struct ds1963s_tool *tool,
	int secret,
	int link)
{
	ds1963s_client_read_auth_page_reply_t auth_reply;
	ds1963s_client_sp_read_reply_t sp_reply;
	struct ds1963s_brute *brute;
	int addr, ret;

	assert(tool != NULL);
	assert(secret >= 0 && secret <= 8);
	assert(link >= 0 && link <= 3);

       	brute = &tool->brute;
	/* Only link 0 is not destructive.  The other 3 links need a partial
	 * overwrite to make things work.
	 */
	if (link != 0) {
		ret = ds1963s_client_secret_write(
			&tool->client,		/* DS1963S context         */
			secret,			/* Secret number           */
			"\0\0\0\0\0\0\0\0",	/* Partial secret to write */
			link * 2		/* Length of the secret    */
		);

		if (ret == -1)
			return -1;
	}

	/* Calculate the address of this secret. */
	addr = ds1963s_client_page_to_address(&tool->client, secret);
	if (addr == -1)
		return -1;

	/* Erase the scratchpad. */
	if (ds1963s_client_sp_erase(&tool->client, 0) == -1)
		return -1;

	/* Read auth. page over the current scratchpad/data/etc. */
	if (ds1963s_client_read_auth(&tool->client, addr, &auth_reply) == -1)
		return -1;

	if (ds1963s_client_sp_read(&tool->client, &sp_reply) == -1)
		return -1;

	memcpy(brute->secrets[secret].target_hmac[link], &sp_reply.data[8], 20);
	return 0;
}

void *
ds1963s_tool_brute_link(struct ds1963s_tool *tool, int secret, int link)
{
	struct ds1963s_brute_secret *secret_state;
	struct ds1963s_brute *brute;
	struct ds1963s_device *dev;
	int i;

 	brute = &tool->brute;

	assert(brute != NULL);
	assert(secret >= 0 && secret <= 8);
	assert(link >= 0 && link <= 3);

 	secret_state = &brute->secrets[secret];
	dev          = &brute->dev;

	/* Set the secret to 0. */
	memset(&dev->secret_memory[secret * 8], 0, 8);

	/* Copy the known part of the secret out for this link type. */
	memcpy(&dev->secret_memory[secret * 8 + link * 2],
	       &secret_state->secret[link * 2],
	       8 - link * 2);

	for (i = 0; i < 65536; i++) {
		int index = secret * 8 + link * 2;

		dev->secret_memory[index]     = (i >> 0) & 0xFF;
		dev->secret_memory[index + 1] = (i >> 8) & 0xFF;

		ds1963s_dev_read_auth_page(dev, secret);

		if (!memcmp(secret_state->target_hmac[link], &dev->scratchpad[8], 20)) {
			secret_state->secret[link * 2] = (i >>  0) & 0xFF;
			secret_state->secret[link * 2 + 1] = (i >>  8) & 0xFF;
			break;
		}

		memset(dev->scratchpad, 0xFF, 32);
	}

	return NULL;
}


void
ds1963s_tool_memory_dump_text(struct ds1963s_tool *tool)
{
	struct ds1963s_client *ctx = &tool->client;
	uint8_t page[32];
	int i, j;

	for (i = 0; i < 16; i++) {
		if (ds1963s_client_memory_read(ctx, 32 * i, page, 32) == -1)
			continue;

		printf("Page #%.2d: ", i);

		for (j = 0; j < sizeof page; j++)
			printf("%.2x", page[j]);
		printf("\n");
	}
}

void
ds1963s_tool_rom_print_text(struct ds1963s_tool *tool, struct ds1963s_rom *rom)
{
	int i;

	printf("64-BIT LASERED ROM\n");
	printf("------------------\n");
	printf("Family: 0x%.2x\n", rom->family);

	printf("Serial: ");
	for (i = 0; i < sizeof rom->serial; i++)
		printf("%.2x", rom->serial[i]);

	printf("\nCRC8  : 0x%.2x\n", rom->crc);
}

void
ds1963s_tool_write_cycle_counters_print_text(struct ds1963s_tool *tool, uint32_t counters[16])
{
	printf("\nWrite cycle counter states\n");
	printf("--------------------------\n");
	printf("Data page  8: 0x%.8x\n", counters[0]);
	printf("Data page  9: 0x%.8x\n", counters[1]);
	printf("Data page 10: 0x%.8x\n", counters[2]);
	printf("Data page 11: 0x%.8x\n", counters[3]);
	printf("Data page 12: 0x%.8x\n", counters[4]);
	printf("Data page 13: 0x%.8x\n", counters[5]);
	printf("Data page 14: 0x%.8x\n", counters[6]);
	printf("Data page 15: 0x%.8x\n", counters[7]);
	printf("Secret     0: 0x%.8x\n", counters[8]);
	printf("Secret     1: 0x%.8x\n", counters[9]);
	printf("Secret     2: 0x%.8x\n", counters[10]);
	printf("Secret     3: 0x%.8x\n", counters[11]);
	printf("Secret     4: 0x%.8x\n", counters[12]);
	printf("Secret     5: 0x%.8x\n", counters[13]);
	printf("Secret     6: 0x%.8x\n", counters[14]);
	printf("Secret     7: 0x%.8x\n", counters[15]);
}

void
ds1963s_tool_info_text(struct ds1963s_tool *tool)
{
	struct ds1963s_rom rom;
	uint32_t counters[16];

	ds1963s_client_rom_get(&tool->client, &rom);
	ds1963s_tool_rom_print_text(tool, &rom);

	if (ds1963s_write_cycle_get_all(&tool->client, counters) != -1)
		ds1963s_tool_write_cycle_counters_print_text(tool, counters);

	printf("\n4096-bit NVRAM dump\n");
	printf("-------------------\n");
	ds1963s_tool_memory_dump_text(tool);

	printf("\nPRNG Counter: 0x%.8x\n",
		ds1963s_client_prng_get(&tool->client));
}

void
ds1963s_tool_info(struct ds1963s_tool *tool, int format)
{
#ifdef HAVE_LIBYAML
	if (format == FORMAT_YAML)
		ds1963s_tool_info_yaml(tool);
	else
#endif
		ds1963s_tool_info_text(tool);
}

int
ds1963s_tool_info_full_disclaimer(void)
{
	int ch;

	fprintf(stderr, "WARNING: This operation will perform partial "
	                "writes to the DS1963S secrets.\n");
	fprintf(stderr, "         Failure may cause the dongle secrets to be "
	                "lost forever.\n\n");
	fprintf(stderr, "Do you wish to proceed? (y/n) ");

	ch = fgetc(stdin);
	return ch == 'y' || ch == 'Y';
}

void
xds1963s_tool_secret_hmac_target_get(
	struct ds1963s_tool *tool,
	int secret,
	int link)
{
	int ret;

	ret = ds1963s_tool_secret_hmac_target_get(tool, secret, link);
	if (ret == -1) {
		ds1963s_client_perror(&tool->client,
			"ds1963s_tool_secret_hmac_target_get()");
		ds1963s_tool_fatal(tool);
	}
}

void
xds1963s_client_memory_read(struct ds1963s_tool *tool,
                            uint16_t address, uint8_t *data, size_t size)
{
	int ret;

	ret = ds1963s_client_memory_read(&tool->client, address, data, size);
	if (ret == -1) {
		ds1963s_client_perror(&tool->client,
			"ds1963s_client_memory_read()");
		ds1963s_tool_fatal(tool);
	}
}

void
xds1963s_client_secret_write(struct ds1963s_tool *tool,
                             int secret, const void *data, size_t len)
{
	int ret;

	ret = ds1963s_client_secret_write(&tool->client, secret, data, len);
	if (ret == -1) {
		ds1963s_client_perror(&tool->client,
			"ds1963s_client_secret_write()");
		ds1963s_tool_fatal(tool);
	}
}

void
ds1963s_tool_secrets_get(struct ds1963s_tool *tool,
                         struct ds1963s_rom *rom,
                         uint32_t counters[16])
{
	uint8_t buf[256];
	uint8_t data[32];

	/* Initialize the brute forcer state. */
	for (int i = 0; i < 8; i++) {
		xds1963s_client_memory_read(
			tool,
			32 * i,
			&buf[32 * i],
			32
		);
	}

	memcpy(tool->brute.dev.serial, &rom->raw[1], 6);
	memcpy(tool->brute.dev.data_memory, buf, sizeof buf);

	/* Initialize the write cycle counters. */
	for (int i = 0; i < 8; i++) {
		tool->brute.dev.data_wc[i]   = counters[i];
		tool->brute.dev.secret_wc[i] = counters[i + 8];
	}

	if (tool->verbose)
		fprintf(stderr, "\n01. Calculating HMAC links.\n");

	for (int secret = 0; secret < 8; secret++) {
		for (int link = 0; link < 4; link++) {
			if (tool->verbose) {
				fprintf(stderr, "\r    Secret #%d [%d/4]",
				        secret, link);
			}

			xds1963s_tool_secret_hmac_target_get(tool, secret, link);
		}

		if (tool->verbose)
			fprintf(stderr, "\r    Secret #%d [4/4]\n", secret);
	}

	if (tool->verbose)
		fprintf(stderr, "02. Calculating secrets from HMAC links.\n");

	for (int secret = 0; secret < 8; secret++) {
		for (int link = 3; link >= 0; link--)
			ds1963s_tool_brute_link(tool, secret, link);
	}

	if (tool->verbose) {
		fprintf(stderr, "03. Restoring recovered keys.\n");
		fprintf(stderr, "    Secret #0-3\n");
	}

	for (int i = 0; i < 4; i++)
		memcpy(&data[i * 8], tool->brute.secrets[i].secret, 8);
	xds1963s_client_secret_write(tool, 0, data, sizeof data);

	if (tool->verbose)
		fprintf(stderr, "    Secret #4-7\n");

	for (int i = 0; i < 4; i++)
		memcpy(&data[i * 8], tool->brute.secrets[i + 4].secret, 8);
	xds1963s_client_secret_write(tool, 4, data, sizeof data);
}

void
ds1963s_tool_info_full_text(struct ds1963s_tool *tool)
{
	struct ds1963s_rom rom;
	uint32_t counters[16];

	ds1963s_client_rom_get(&tool->client, &rom);
	ds1963s_tool_rom_print_text(tool, &rom);

	if (ds1963s_write_cycle_get_all(&tool->client, counters) != -1)
		ds1963s_tool_write_cycle_counters_print_text(tool, counters);

	printf("\n4096-bit NVRAM dump\n");
	printf("-------------------\n");
	ds1963s_tool_memory_dump_text(tool);

	printf("\nPRNG Counter: 0x%.8x\n",
		ds1963s_client_prng_get(&tool->client));

	ds1963s_tool_secrets_get(tool, &rom, counters);

	printf("\nSecrets dump\n");
	printf("------------\n");

	for (int secret = 0; secret < 8; secret++) {
		printf("Secret %d: ", secret);
		for (int i = 0; i < 8; i++)
			printf("%.2x", tool->brute.secrets[secret].secret[i]);
		printf("\n");
	}
}

void
ds1963s_tool_info_full(struct ds1963s_tool *tool, int format)
{
	if (ds1963s_tool_info_full_disclaimer() == 0)
		return;

#ifdef HAVE_LIBYAML
	if (format == FORMAT_YAML)
		ds1963s_tool_info_full_yaml(tool);
	else
#endif
		ds1963s_tool_info_full_text(tool);
}

void
ds1963s_tool_write(struct ds1963s_tool *tool, uint16_t address,
                   const uint8_t *data, size_t len)
{
	struct ds1963s_client *ctx = &tool->client;

	if (ds1963s_client_memory_write(ctx, address, data, len) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_memory_write()");
		ds1963s_tool_fatal(tool);
	}
}

void
ds1963s_tool_secret_write(struct ds1963s_tool *tool, int secret,
                          const uint8_t *data, size_t len)
{
	struct ds1963s_client *ctx = &tool->client;

	if (ds1963s_client_secret_write(ctx, secret, data, len) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_secret_write()");
		ds1963s_tool_fatal(tool);
	}
}

void
ds1963s_tool_secret_first_set(struct ds1963s_tool *tool, int secret, int page)
{
	struct ds1963s_client *ctx = &tool->client;
	SHACopr *copr = &ctx->copr;
	int ret;

	if (secret < 0 || secret > 7) {
		ctx->errno = DS1963S_ERROR_SECRET_NUM;
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}

	/* We check if the page is valid before erasing the scratchpad. */
	if (ds1963s_client_page_to_address(ctx, page) == -1) {
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}

	/* XXX: for now we only support this with an erased scratchpad. */
	if (ds1963s_client_sp_erase(ctx, 0) == -1) {
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}

	if (ds1963s_client_secret_compute_first(ctx, page) == -1) {
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}

	/* Generated secret is on scratchpad, copy it to the secret. */
	ret = CopySecretSHA18(copr->portnum, secret);
	if (ret == FALSE) {
		ctx->errno = DS1963S_ERROR_COPY_SECRET;
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}
}

void
ds1963s_tool_secret_next_set(struct ds1963s_tool *tool, int secret, int page)
{
	struct ds1963s_client *ctx = &tool->client;
	SHACopr *copr = &ctx->copr;
	int ret;

	if (secret < 0 || secret > 7) {
		ctx->errno = DS1963S_ERROR_SECRET_NUM;
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}

	/* We check if the page is valid before erasing the scratchpad. */
	if (ds1963s_client_page_to_address(ctx, page) == -1) {
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}

	/* XXX: for now we only support this with an erased scratchpad. */
	if (ds1963s_client_sp_erase(ctx, 0) == -1) {
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}

	if (ds1963s_client_secret_compute_next(ctx, page) == -1) {
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}

	/* Generated secret is on scratchpad, copy it to the secret. */
	ret = CopySecretSHA18(copr->portnum, secret);
	if (ret == FALSE) {
		ctx->errno = DS1963S_ERROR_COPY_SECRET;
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}
}

void
ds1963s_tool_validate_data_page(struct ds1963s_tool *tool, int page)
{
	struct ds1963s_client *ctx = &tool->client;

	if (ds1963s_client_validate_data_page(ctx, page) == -1) {
		ds1963s_client_perror(ctx, "%s()", __FUNCTION__);
		ds1963s_tool_fatal(tool);
	}
}

void
ds1963s_tool_read(struct ds1963s_tool *tool, uint16_t address, size_t size)
{
	struct ds1963s_client *ctx = &tool->client;
	uint8_t data[32];
	size_t i;

	if (ds1963s_client_sp_erase(ctx, 0) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_sp_erase()");
		ds1963s_tool_fatal(tool);
	}

	/* Will not read more than 32 bytes. */
	if (ds1963s_client_memory_read(ctx, address, data, size) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_memory_read()");
		ds1963s_tool_fatal(tool);
	}

	for (i = 0; i < size; i++)
		printf("%c", data[i]);
}

void
ds1963s_tool_read_auth(struct ds1963s_tool *tool, int page, size_t size)
{
	ds1963s_client_read_auth_page_reply_t auth_reply;
	ds1963s_client_sp_read_reply_t sp_reply;
	struct ds1963s_client *ctx;
	int addr;
	int i;

	assert(tool != NULL);

       	ctx = &tool->client;
	if ( (addr = ds1963s_client_page_to_address(ctx, page)) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_page_to_address()");
		ds1963s_tool_fatal(tool);
	}

	if (ds1963s_client_sp_erase(ctx, 0) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_sp_erase()");
		ds1963s_tool_fatal(tool);
	}

	if (ds1963s_client_read_auth(ctx, addr, &auth_reply) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_read_auth()");
		ds1963s_tool_fatal(tool);
	}

	if (ds1963s_client_sp_read(ctx, &sp_reply) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_sp_read()");
		ds1963s_tool_fatal(tool);
	}

	printf("Read authenticated page #%.2d\n", page);
	printf("---------------------------\n");
	printf("Data write cycle counter  : 0x%.8x\n", auth_reply.data_wc);
	printf("Secret write cycle counter: 0x%.8x\n", auth_reply.secret_wc);
	printf("CRC16                     : 0x%.4x\n", auth_reply.crc16);
	printf("Data                      : ");

	for (i = 0; i < 32; i++)
		printf("%.2x", auth_reply.data[i]);
	printf("\n");

	printf("SHA1 hash                 : ");
	ds1963s_client_hash_print(&sp_reply.data[8]);
}

void
ds1963s_tool_sign(struct ds1963s_tool *tool, int page, size_t size)
{
	struct ds1963s_client *ctx;
	unsigned char hash[20];
	int addr;

	assert(tool != NULL);

	ctx = &tool->client;

	if ( (addr = ds1963s_client_page_to_address(ctx, page)) == -1) {
		ds1963s_client_perror(ctx, NULL);
		ds1963s_tool_fatal(tool);
	}

	/* XXX: signing happens over scratchpad data.  We may not want
	 * to clear this here but rather let the state persist.
	 */
        /* Erase the scratchpad to clear the HIDE flag. */
	if (ds1963s_client_sp_erase(ctx, 0) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_sp_erase()");
		ds1963s_tool_fatal(tool);
	}

	if (ds1963s_client_sign_data_page(ctx, addr) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_sign_data_page()");
		ds1963s_tool_fatal(tool);
	}

	if (ds1963s_client_hash_read(&tool->client, hash) == -1) {
		ds1963s_client_perror(ctx, "ds1963s_client_hash_read()");
		ds1963s_tool_fatal(tool);
	}

	printf("Sign data page #%.2d\n", page);
	printf("---------------------------\n");
	printf("SHA1 hash: ");
	ds1963s_client_hash_print(hash);
}

void
usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [OPTION] [DATA]\n",
		progname ? progname : "ds1963s-tool");

	fprintf(stderr, "Modifiers can be used for several commands.\n");
	fprintf(stderr, "   -a --address=address  the memory address used "
	                "in several functions.\n");
	fprintf(stderr, "   -d --device=pathname  the serial device used.\n");
	fprintf(stderr, "   -p --page=pagenum     the page number used in "
	                "several functions.\n");
	fprintf(stderr, "   -v --verbose          verbose operation.\n");

	fprintf(stderr, "\nFunction that will be performed.\n");
	fprintf(stderr, "   -i --info                print ibutton information.\n");
	fprintf(stderr, "   -f --info-full           print full ibutton information.\n");
	fprintf(stderr, "   -r --read=size           read 'size' bytes of data.\n");
	fprintf(stderr, "   -t --read-auth=size      read 'size' bytes of "
	                "authenticated data.\n");
	fprintf(stderr, "   -s --sign-data=size      sign 'size' bytes of data.\n");
	fprintf(stderr, "   -w --write=hex_data      write data.\n");
	fprintf(stderr, "   --secret-set-first=n     compute first secret and write it to secret 'n'.\n");
	fprintf(stderr, "   --secret-set-next=n      compute next secret and write it to secret 'n'.\n");
	fprintf(stderr, "   --write-secret=n         write data to secret 'n'.\n");
}

static const struct option options[] =
{
	{ "address",		  1,	NULL,	'a' },
	{ "device",		  1,	NULL,	'd' },
	{ "help",		  0,	NULL,	'h' },
	{ "page",		  1,	NULL,	'p' },
	{ "info",		  0,	NULL,	'i' },
	{ "info-full",		  0,	NULL,	'f' },
	{ "read",		  1,	NULL,	'r' },
	{ "read-auth",		  1,	NULL,	't' },
	{ "secret-set-first",     1,    NULL,    0  },
	{ "secret-set-next",      1,    NULL,    0  },
	{ "sign-data",		  1,	NULL,	's' },
	{ "validate",		  0,	NULL,	 0  },
	{ "verbose",              0,    NULL,   'v' },
	{ "write",		  0,	NULL,	'w' },
	{ "write-secret",	  1,	NULL,	 0  },
	{ NULL,			  0,	NULL,	 0  }
};

const char optstr[] = "a:d:hr:p:s:ifvwy";

int
main(int argc, char **argv)
{
	const char *device_name = DEFAULT_SERIAL_PORT;
	struct ds1963s_tool tool;
	int address, page, size;
	int mask, mode, o;
	uint8_t data[32];
	int verbose;
	size_t len;
	int format;
	int secret;
	int i;

	len = mode = verbose = 0;
	format = FORMAT_TEXT;
	address = page = secret = size = -1;
	while ( (o = getopt_long(argc, argv, optstr, options, &i)) != -1) {
		switch (o) {
		case 0:
			if (!strcmp(options[i].name, "write-secret")) {
				mode = MODE_WRITE_SECRET;
				secret = atoi(optarg);
				break;
			} else if (!strcmp(options[i].name, "secret-set-first")) {
				mode = MODE_SECRET_FIRST_SET;
				secret = atoi(optarg);
				break;
			} else if (!strcmp(options[i].name, "secret-set-next")) {
				mode = MODE_SECRET_NEXT_SET;
				secret = atoi(optarg);
				break;
			} else if (!strcmp(options[i].name, "validate")) {
				mode = MODE_VALIDATE_DATA_PAGE;
				break;
			}
			break;
		case 'a':
			address = atoi(optarg);
			page = ds1963s_client_address_to_page(&tool.client, address);
			if (page == -1) {
				ds1963s_client_perror(&tool.client, NULL);
				exit(EXIT_FAILURE);
			}
			break;
		case 'd':
			device_name = optarg;
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		case 'r':
			mode = MODE_READ;
			size = atoi(optarg);
			break;
		case 'p':
			page = atoi(optarg);
			address = ds1963s_client_page_to_address(&tool.client, page);
			if (address == -1) {
				ds1963s_client_perror(&tool.client, NULL);
				exit(EXIT_FAILURE);
			}
			break;
		case 's':
			mode = MODE_SIGN;
			break;
		case 'i':
			mode = MODE_INFO;
			break;
		case 'f':
			mode = MODE_INFO_FULL;
			break;
		case 't':
			mode = MODE_READ_AUTH;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			mode = MODE_WRITE;
			break;
		case 'y':
			format = FORMAT_YAML;
			break;
		}
	}

	if (mode == 0) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Verify that the options we have processed make sense. */
	mask = MODE_READ | MODE_READ_AUTH | MODE_WRITE;
	if ( (mode & mask) != 0 && address == -1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	mask = MODE_SECRET_FIRST_SET;
	if ( (mode & mask) != 0 && page == -1) {
		fprintf(stderr, "--secret-set-first expects a -p/--page argument.\n");
		exit(EXIT_FAILURE);
	}

       	mask = MODE_SECRET_NEXT_SET;
	if ( (mode & mask) != 0 && page == -1) {
		fprintf(stderr, "--secret-set-next expects a -p/--page argument.\n");
		exit(EXIT_FAILURE);
	}

	mask = MODE_VALIDATE_DATA_PAGE;
	if ( (mode & mask) != 0 && page == -1) {
		fprintf(stderr, "--validate expects a -p/--page argument.\n");
		exit(EXIT_FAILURE);
	}

	/* Write functions take an extra argument. */
	mask = MODE_WRITE | MODE_WRITE_SECRET;
	if ( (mode & mask) != 0) {
		if (optind >= argc) {
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}

		if (hex_decode(data, argv[optind], 32) == -1) {
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}

		len = strlen(argv[optind]) / 2;
	}

	/* Pre-check if the serial device is accessible. */
	if (access(device_name, R_OK | W_OK) != 0) {
		fprintf(stderr, "Cannot access %s\n", device_name);
		exit(EXIT_FAILURE);
	}

	/* Initialize the DS1963S device. */
	if (ds1963s_tool_init(&tool, device_name) == -1) {
		ds1963s_client_perror(&tool.client, "ds1963s_init()");
		exit(EXIT_FAILURE);
	}
	tool.verbose = verbose;

	switch (mode) {
	case MODE_INFO:
		ds1963s_tool_info(&tool, format);
		break;
	case MODE_INFO_FULL:
		ds1963s_tool_info_full(&tool, format);
		break;
	case MODE_READ:
		ds1963s_tool_read(&tool, address, size);
		break;
	case MODE_READ_AUTH:
		ds1963s_tool_read_auth(&tool, page, size);
		break;
	case MODE_SIGN:
		ds1963s_tool_sign(&tool, page, size);
		break;
	case MODE_WRITE:
		ds1963s_tool_write(&tool, address, data, len);
		break;
	case MODE_WRITE_SECRET:
		ds1963s_tool_secret_write(&tool, secret, data, len);
		break;
	case MODE_SECRET_FIRST_SET:
		ds1963s_tool_secret_first_set(&tool, secret, page);
		break;
	case MODE_SECRET_NEXT_SET:
		ds1963s_tool_secret_next_set(&tool, secret, page);
		break;
	case MODE_VALIDATE_DATA_PAGE:
		ds1963s_tool_validate_data_page(&tool, page);
		break;
	}

	ds1963s_tool_destroy(&tool);
	exit(EXIT_SUCCESS);
}
