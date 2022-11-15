/* getput.h
 *
 * Macros for integer (de)serialization.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * Copyright (C) 2007-2019  Ronald Huizer <rhuizer@hexpedition.com>
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

#ifndef GETPUT_H
#define GETPUT_H

#include <stdint.h>

#define GET_FLOAT_MSB(cp)	GET_32BIT_MSB(cp)

#define GET_64BIT_MSB(cp)						\
	(((uint64_t)(uint8_t)(cp)[0] << 56) |				\
	 ((uint64_t)(uint8_t)(cp)[1] << 48) |				\
	 ((uint64_t)(uint8_t)(cp)[2] << 40) |				\
	 ((uint64_t)(uint8_t)(cp)[3] << 32) |				\
	 ((uint64_t)(uint8_t)(cp)[4] << 24) |				\
	 ((uint64_t)(uint8_t)(cp)[5] << 16) |				\
	 ((uint64_t)(uint8_t)(cp)[6] << 8) |				\
	 ((uint64_t)(uint8_t)(cp)[7]))

#define GET_32BIT_MSB(cp)						\
	(((uint32_t)(uint8_t)(cp)[0] << 24) |				\
	 ((uint32_t)(uint8_t)(cp)[1] << 16) |				\
	 ((uint32_t)(uint8_t)(cp)[2] << 8) |				\
	 ((uint32_t)(uint8_t)(cp)[3]))

#define GET_24BIT_MSB(cp)						\
	(((uint32_t)(uint8_t)(cp)[0] << 16) |				\
	 ((uint32_t)(uint8_t)(cp)[1] << 8) |				\
	 ((uint32_t)(uint8_t)(cp)[2]))

#define GET_16BIT_MSB(cp)						\
	(((uint16_t)(uint8_t)(cp)[0] << 8) |				\
	 ((uint16_t)(uint8_t)(cp)[1]))

#define PUT_FLOAT_MSB(cp, value)					\
	PUT_32BIT_MSB(cp, *(uint32_t *)&value)
#define PUT_DOUBLE_MSB(cp, value)					\
	PUT_64BIT_MSB(cp, *(uint64_t *)&value)

#define PUT_64BIT_MSB(cp, value)					\
	do {								\
		(cp)[0] = (value) >> 56;				\
		(cp)[1] = (value) >> 48;				\
		(cp)[2] = (value) >> 40;				\
		(cp)[3] = (value) >> 32;				\
		(cp)[4] = (value) >> 24;				\
		(cp)[5] = (value) >> 16;				\
		(cp)[6] = (value) >> 8;					\
		(cp)[7] = (value);					\
	} while (0)

#define PUT_32BIT_MSB(cp, value)					\
	do {								\
		(cp)[0] = (value) >> 24;				\
		(cp)[1] = (value) >> 16;				\
		(cp)[2] = (value) >> 8;					\
		(cp)[3] = (value);					\
	} while (0)

#define PUT_24BIT_MSB(cp, value)					\
	do {								\
		(cp)[0] = (value) >> 16;				\
		(cp)[1] = (value) >> 8;					\
		(cp)[2] = (value);					\
	} while (0)

#define PUT_16BIT_MSB(cp, value)					\
	do {								\
		(cp)[0] = (value) >> 8;					\
		(cp)[1] = (value);					\
	} while (0)

#define GET_FLOAT_LSB(cp)	GET_32BIT_LSB(cp)
#define GET_DOUBLE_LSB(cp)	GET_64BIT_LSB(cp)

#define GET_64BIT_LSB(cp)						\
	(((uint64_t)(uint8_t)(cp)[7] << 56) |				\
	 ((uint64_t)(uint8_t)(cp)[6] << 48) |				\
	 ((uint64_t)(uint8_t)(cp)[5] << 40) |				\
	 ((uint64_t)(uint8_t)(cp)[4] << 32) |				\
	 ((uint64_t)(uint8_t)(cp)[3] << 24) |				\
	 ((uint64_t)(uint8_t)(cp)[2] << 16) |				\
	 ((uint64_t)(uint8_t)(cp)[1] << 8) |				\
	 ((uint64_t)(uint8_t)(cp)[0]))

#define GET_32BIT_LSB(cp)						\
	(((uint32_t)(uint8_t)(cp)[3] << 24) |				\
	 ((uint32_t)(uint8_t)(cp)[2] << 16) |				\
	 ((uint32_t)(uint8_t)(cp)[1] << 8) |				\
	 ((uint32_t)(uint8_t)(cp)[0]))

#define GET_24BIT_LSB(cp)						\
	(((uint32_t)(uint8_t)(cp)[2] << 16) |				\
	 ((uint32_t)(uint8_t)(cp)[1] << 8) |				\
	 ((uint32_t)(uint8_t)(cp)[0]))

#define GET_16BIT_LSB(cp)						\
	(((uint16_t)(uint8_t)(cp)[1] << 8) |				\
	 ((uint16_t)(uint8_t)(cp)[0]))

#define PUT_FLOAT_LSB(cp, value)					\
	PUT_32BIT_LSB(cp, *(uint32_t *)&value)
#define PUT_DOUBLE_LSB(cp, value)					\
	PUT_64BIT_LSB(cp, *(uint64_t *)&value)

#define PUT_64BIT_LSB(cp, value)					\
	do {								\
		(cp)[7] = (value) >> 56;				\
		(cp)[6] = (value) >> 48;				\
		(cp)[5] = (value) >> 40;				\
		(cp)[4] = (value) >> 32;				\
		(cp)[3] = (value) >> 24;				\
		(cp)[2] = (value) >> 16;				\
		(cp)[1] = (value) >> 8;					\
		(cp)[0] = (value);					\
	} while (0)

#define PUT_32BIT_LSB(cp, value)					\
	do {								\
		(cp)[3] = (value) >> 24;				\
		(cp)[2] = (value) >> 16;				\
		(cp)[1] = (value) >> 8;					\
		(cp)[0] = (value);					\
	} while (0)

#define PUT_24BIT_LSB(cp, value)					\
	do {								\
		(cp)[2] = (value) >> 16;				\
		(cp)[1] = (value) >> 8;					\
		(cp)[0] = (value);					\
	} while (0)

#define PUT_16BIT_LSB(cp, value)					\
	do {								\
		(cp)[1] = (value) >> 8;					\
		(cp)[0] = (value);					\
	} while (0)

#endif	/* !GETPUT_H */
