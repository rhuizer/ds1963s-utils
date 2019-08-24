/* ds1963s-common.c
 *
 * A ds1963s emulation and utility framework.
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
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include "ds1963s-common.h"

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

static uint8_t oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

uint16_t
ds1963s_crc16_update(uint16_t crc16, const uint8_t *buf, size_t count)
{
	uint16_t crc = crc16;
	uint16_t word;
	size_t i;

	for (i = 0; i < count; i++) {
		word = (crc ^ buf[i]) & 0xFF;
		crc >>= 8;

		if (oddparity[word & 0xf] ^ oddparity[word >> 4])
			crc ^= 0xc001;

		word <<= 6;
		crc ^= word;
		word <<= 1;
		crc ^= word;
	}

	return crc;
}

uint16_t
ds1963s_crc16_update_byte(uint16_t crc16, uint8_t byte)
{
	return ds1963s_crc16_update(crc16, &byte, 1);
}

uint16_t
ds1963s_crc16(const uint8_t *buf, size_t count)
{
	return ds1963s_crc16_update(0, buf, count);
}

uint16_t
ds1963s_ta_to_address(uint8_t TA1, uint8_t TA2)
{
        return (TA2 << 8) | TA1;
}

void
ds1963s_address_to_ta(uint16_t address, uint8_t *TA1, uint8_t *TA2)
{
	*TA1 = address & 0xFF;
	*TA2 = address >> 8;
}

int
ds1963s_address_to_page(int address)
{
        if (address < 0 || address >= 0x400)
                return -1;

        return address / 32;
}

int
ds1963s_address_secret(int address)
{
	return address >= 0x200 && address <= 0x23F;
}

static inline int
__dehex_char(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	c = tolower(c);

	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';

	return -1;
}

ssize_t
hex_decode(uint8_t *dst, const char *src, size_t n)
{
	size_t src_len = strlen(src);
	size_t i, j;

	if (src_len == 0 || src_len % 2 != 0)
		return -1;

	for (i = 0; i < src_len; i++)
		if (!isxdigit(src[i]))
			return -1;

	for (i = j = 0; i < src_len - 1 && j < n; i += 2, j++)
		dst[j] = __dehex_char(src[i]) * 16 + __dehex_char(src[i + 1]);

	return j;
}

void
fhexdump(FILE *fp, uint8_t *p, size_t n, uint64_t offset)
{
	assert(fp != NULL);
	assert(p != NULL);

	if (n == 0)
		return;

	while (n != 0) {
		char buf[17] = { 0 };
		int  i;

		fprintf(fp, "%.8" PRIx64 ":", offset);
		for (i = 0; i < 16; i++, offset++) {
			if (i % 2 == 0)
				fprintf(fp, " ");

			if (n != 0)  {
				n = n - 1;
				if (*p >= 0x20 && *p <= 0x7E)
					buf[i] = *p;
				else
					buf[i] = '.';

				fprintf(fp, "%.2x", *p++);
			} else {
				fprintf(fp, "  ");
			}
		}

		fprintf(fp, "  %s\n", buf);
	}
}

void
hexdump(uint8_t *p, size_t n, uint64_t offset)
{
	fhexdump(stdout, p, n, offset);
}
