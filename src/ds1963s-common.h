/* ds1963s-common.h
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
#ifndef DS1963S_COMMON_H
#define DS1963S_COMMON_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t  ds1963s_crc8(const uint8_t *buf, size_t count);
uint16_t ds1963s_crc16(const uint8_t *buf, size_t count);
uint16_t ds1963s_crc16_update(uint16_t crc16, const uint8_t *buf, size_t count);
uint16_t ds1963s_crc16_update_byte(uint16_t crc16, uint8_t byte);

uint16_t ds1963s_ta_to_address(uint8_t TA1, uint8_t TA2);
void     ds1963s_address_to_ta(uint16_t address, uint8_t *TA1, uint8_t *TA2);
int      ds1963s_address_to_page(int address);
int      ds1963s_address_secret(int address);

ssize_t  hex_decode(uint8_t *dst, const char *src, size_t n);
void     hexdump(uint8_t *s, size_t n, uint64_t offset);
void     fhexdump(FILE *fp, uint8_t *s, size_t n, uint64_t offset);

#ifdef __cplusplus
};
#endif

#endif
