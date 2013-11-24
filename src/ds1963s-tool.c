/* ds1963s-tool.c
 *
 * A utility for accessing the ds1963s iButton over 1-wire RS-232.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 *  -- Ronald Huizer, 2013
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include "ds1963s.h"

#define DEFAULT_SERIAL_PORT	"/dev/ttyUSB0"
#define MODE_INFO		0
#define MODE_READ		1
#define MODE_READ_AUTH		2
#define MODE_WRITE		4

void ds1963s_tool_fatal(struct ds1963s_client *ctx)
{
	ds1963s_client_destroy(ctx);
	exit(EXIT_FAILURE);
}

void ds1963s_tool_memory_dump(struct ds1963s_client *ctx)
{
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

void ds1963s_tool_info(struct ds1963s_client *ctx)
{
	struct ds1963s_rom rom;
	uint32_t counters[16];
	int i;

	if (ds1963s_client_rom_get(ctx, &rom) != -1) {
		printf("64-BIT LASERED ROM\n");
		printf("------------------\n");
		printf("Family: 0x%.2x\n", rom.family);
		printf("Serial: ");

		for (i = 0; i < sizeof(rom.serial); i++)
			printf("%.2x", rom.serial[i]);
		printf("\nCRC8  : 0x%.2x\n", rom.crc);
	}

	if (ds1963s_write_cycle_get_all(ctx, counters) != -1) {
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

	printf("\n4096-bit NVRAM dump\n");
	printf("-------------------\n");
	ds1963s_tool_memory_dump(ctx);

	printf("\nPRNG Counter: 0x%.8x\n", ds1963s_client_prng_get(ctx));
}

void ds1963s_tool_write(struct ds1963s_client *ctx, uint16_t address,
                        const char *data)
{
	size_t size = strlen(data);

	if (ds1963s_client_memory_write(ctx, address, data, size) == -1) {
		ibutton_perror("ds1963s_client_memory_write()");
		ds1963s_tool_fatal(ctx);
	}
}

void
ds1963s_tool_read(struct ds1963s_client *ctx, uint16_t address, size_t size)
{
	int portnum = ctx->copr.portnum;
	uint8_t data[32];
	size_t i;

        EraseScratchpadSHA18(portnum, 0xFFFF, FALSE);

	ds1963s_client_taes_print(ctx);

	/* Will not read more than 32 bytes. */
	if (ds1963s_client_memory_read(ctx, address, data, size) == -1) {
		ibutton_perror("ds1963s_client_memory_read()");
		ds1963s_tool_fatal(ctx);
	}
	ds1963s_client_taes_print(ctx);

	for (i = 0; i < size; i++)
		printf("%c", data[i]);
}

void
ds1963s_tool_read_auth(struct ds1963s_client *ctx, int page, size_t size)
{
	int addr = ds1963s_client_page_to_address(page);
	struct ds1963s_read_auth_page_reply reply;
	int portnum = ctx->copr.portnum;
	uint8_t data[32];
	uint8_t hash[20];
	int i;

        /* Erase the scratchpad to clear the HIDE flag. */
        if (EraseScratchpadSHA18(portnum, 0, FALSE) == FALSE) {
		fprintf(stderr, "EraseScratchpadSHA18 failed\n");
		exit(EXIT_FAILURE);
	}

	if (ds1963s_client_read_auth(ctx, addr, &reply, 0) == -1) {
		ibutton_perror("ds1963s_client_read_auth()");
		exit(EXIT_FAILURE);
	}

	printf("Read authenticated page #%.2d\n", page);
	printf("---------------------------\n");
	printf("Data write cycle counter  : 0x%.8x\n", reply.data_wc);
	printf("Secret write cycle counter: 0x%.8x\n", reply.secret_wc);
	printf("CRC16                     : 0x%.4x\n", reply.crc16);
	printf("Data                      : ");

	for (i = 0; i < 32; i++)
		printf("%.2x", reply.data[i]);
	printf("\n");

	printf("SHA1 hash                 : ");
	ds1963s_client_hash_print(reply.signature);
}

void usage(const char *progname)
{
	fprintf(stderr, "Use as: %s\n",
		progname ? progname : "ds1963s-tool");
}

static const struct option options[] =
{
	{ "address",		1,	NULL,	'a' },
	{ "device",		1,	NULL,	'd' },
	{ "page",		1,	NULL,	'p' },
	{ "info",		0,	NULL,	'i' },
	{ "read",		1,	NULL,	'r' },
	{ "read-auth",		1,	NULL,	't' },
	{ "write",		1,	NULL,	'w' },
	{ NULL,			0,	NULL,	0   }
};

const char optstr[] = "a:d:r:p:iw:";

int main(int argc, char **argv)
{
	const char *device_name = DEFAULT_SERIAL_PORT;
	struct ds1963s_client ctx;
	int address, page, size;
	int i, ret, mode, opt;
	char *data;
	int mask;

	address = -1;
	while ( (opt = getopt_long(argc, argv, optstr, options, NULL)) != -1) {
		switch (opt) {
		case 'a':
			address = atoi(optarg);
			page = ds1963s_client_address_to_page(address);
			break;
		case 'd':
			device_name = optarg;
			break;
		case 'r':
			mode = MODE_READ;
			size = atoi(optarg);
			break;
		case 'p':
			page = atoi(optarg);
			address = ds1963s_client_page_to_address(page);
			break;
		case 'i':
			mode = MODE_INFO;
			break;
		case 't':
			mode = MODE_READ_AUTH;
			break;
		case 'w':
			data = optarg;
			mode = MODE_WRITE;
			break;
		}
	}

	/* Verify that the options we have processed make sense. */
	mask = MODE_READ | MODE_READ_AUTH | MODE_WRITE;
	if ( (mode & mask) != 0 && address == -1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Pre-check if the serial device is accessible. */
	if (access(device_name, R_OK | W_OK) != 0) {
		fprintf(stderr, "Cannot access %s\n", device_name);
		exit(EXIT_FAILURE);
	}

	/* Initialize the DS1963S device. */
	if (ds1963s_client_init(&ctx, device_name) == -1) {
		ibutton_perror("ds1963s_init()");
		exit(EXIT_FAILURE);
	}

	switch (mode) {
	case MODE_INFO:
		ds1963s_tool_info(&ctx);
		break;
	case MODE_READ:
		ds1963s_tool_read(&ctx, address, size);
		break;
	case MODE_READ_AUTH:
		ds1963s_tool_read_auth(&ctx, page, size);
		break;
	case MODE_WRITE:
		ds1963s_tool_write(&ctx, address, data);
		break;
	}

	ds1963s_client_destroy(&ctx);
	exit(EXIT_SUCCESS);
}
