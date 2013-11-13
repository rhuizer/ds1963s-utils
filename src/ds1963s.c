#include <assert.h>
#include <stdint.h>
#include "getput.h"
#include "ds1963s.h"
#include "ibutton/ownet.h"
#include "ibutton/shaib.h"

int ds1963s_client_init(struct ds1963s_client *ctx, const char *device)
{
	SHACopr *copr = &ctx->copr;

	/* Get port. */
	if ( (copr->portnum = owAcquireEx(device)) == -1)
		return -1;

	/* Find DS1963S iButton. */
	if (FindNewSHA(copr->portnum, copr->devAN, TRUE) == FALSE)
		return -1;

	/* Default the resume flag to FALSE. */
	ctx->resume = 0;
}

void ds1963s_client_destroy(struct ds1963s_client *ctx)
{
	owRelease(ctx->copr.portnum);
}

int ds1963s_client_page_to_address(int page)
{
	/* XXX: set error. */
	if (page < 0 || page > 21)
		return -1;

	return page * 32;
}

int ds1963s_client_address_to_page(int address)
{
	/* XXX: set error. */
	if (address < 0 || address > 0x2c0)
		return -1;

	return address / 32;
}

int ds1963s_client_rom_get(struct ds1963s_client *ctx, struct ds1963s_rom *rom)
{
	int portnum = ctx->copr.portnum;
	uint8_t data[8];

	owSerialNum(portnum, data, TRUE);

	if (ds1963s_crc8(data, 8) != 0)
		return -1;

	rom->family = data[0];
	memcpy(rom->serial, &data[1], 6);
	rom->crc = data[7];

	return 0;
}

int ds1963s_client_serial_get(struct ds1963s_client *ctx, uint8_t serial[6])
{
	struct ds1963s_rom rom;

	if (ds1963s_client_rom_get(ctx, &rom) == -1)
		return -1;

	memcpy(serial, rom.serial, 6);

	return 0;
}

#if 0
int ibutton_secret_zero(int portnum, int pagenum, int secretnum, int resume)
{
	char secret_scratchpad[32];
	int addr = pagenum << 5;
	char secret_data[32];

	memset(secret_scratchpad, 0, 32);
	memset(secret_data, 0, 32);

	if (WriteDataPageSHA18(portnum, pagenum, secret_data, resume) == FALSE)
		return -1;

	if (WriteScratchpadSHA18(portnum, addr, secret_scratchpad, 32, TRUE) == FALSE)
		return -1;

	if (SHAFunction18(portnum, (uchar)SHA_COMPUTE_FIRST_SECRET, addr, TRUE) == FALSE)
		return -1;

	if (CopySecretSHA18(portnum, secretnum) == FALSE)
		return -1;

	return 0;
}

#endif

int
ds1963s_scratchpad_write(struct ds1963s_client *ctx, uint16_t address,
                         const char *data, size_t len)
{
	return ds1963s_scratchpad_write_resume(ctx, address, data, len, 0);
}

int
ds1963s_scratchpad_write_resume(struct ds1963s_client *ctx, uint16_t address,
                                const char *data, size_t len, int resume)
{
	int portnum = ctx->copr.portnum;
	uint8_t buf[64];
	int i = 0;

	/* Make sure the data we provide fits. */
	if (len > 32)
		return -1;

	if (ctx->resume)
		buf[i++] = ROM_CMD_RESUME;
	else 
		OWASSERT(SelectSHA(portnum), OWERROR_ACCESS_FAILED, -1);

	/* write scratchpad command */
	buf[i++] = CMD_WRITE_SCRATCHPAD;
	/* TA1, which fully holds the offset. */
	buf[i++] = address & 0xFF;
	/* TA2, which is unused. */
	buf[i++] = address >> 8;
	/* payload */
	memcpy(buf + i, data, len);

	OWASSERT(owBlock(portnum, 0, buf, len + i), OWERROR_BLOCK_FAILED, -1);
	owTouchReset(portnum);

	return 0;
}

int ds1963s_client_memory_read(struct ds1963s_client *ctx, uint16_t address,
                               uint8_t *data, size_t size)
{
	int portnum = ctx->copr.portnum;
	uint8_t block[35];
	int i;

	/* XXX: need to check max read size. */
	if (size > sizeof(block) - 3)
		return -1;

	OWASSERT(SelectSHA(portnum), OWERROR_ACCESS_FAILED, -1);

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

	/* XXX: set error. */
	if (size > 32)
		return -1;

	/* Erase the scratchpad to clear the HIDE flag. */
	OWASSERT(EraseScratchpadSHA18(portnum, address, FALSE),
	         OWERROR_ERASE_SCRATCHPAD_FAILED, -1);

	/* Write the provided data to the scratchpad. */
	ret = ds1963s_scratchpad_write_resume(ctx, address, data, size, 1);
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

inline int __write_cycle_address(int write_cycle_type)
{
	assert(write_cycle_type >= WRITE_CYCLE_DATA_8);
	assert(write_cycle_type <= WRITE_CYCLE_SECRET_7);

	return 0x260 + write_cycle_type * 4;
}

uint32_t ibutton_write_cycle_get(int portnum, int write_cycle_type)
{
	int address = __write_cycle_address(write_cycle_type);
	uint8_t block[7];

	OWASSERT(SelectSHA(portnum), OWERROR_ACCESS_FAILED, FALSE);

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

	OWASSERT(SelectSHA(portnum), OWERROR_ACCESS_FAILED, -1);

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

	OWASSERT(SelectSHA(portnum), OWERROR_ACCESS_FAILED, -1);

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

void ibutton_perror(const char *s)
{
	if (s)
		fprintf(stderr, "%s: ", s);

	fprintf(stderr, "%s\n", owGetErrorMsg(owGetErrorNum()));
}

static uint8_t ds1963s_crc8_table[] = {
	0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

uint8_t ds1963s_crc8(const uint8_t *buf, size_t count)
{
	uint8_t crc = 0;
	size_t i;

	for (i = 0; i < count; i++)
		crc = ds1963s_crc8_table[crc ^ buf[i]];

	return crc;
}
