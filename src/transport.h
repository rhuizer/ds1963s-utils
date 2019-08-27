/* transport.h
 *
 * Transport layer base class.
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
#ifndef __TRANSPORT_H
#define __TRANSPORT_H

#include <stdio.h>

#define TRANSPORT_ERROR_NONE		0
#define TRANSPORT_ERROR_UNSUPPORTED	1
#define TRANSPORT_ERROR_DESTROY		2

struct transport;

struct transport_operations
{
	int     (*destroy)(struct transport *);
	ssize_t (*read)(struct transport *, void *buf, size_t size);
	ssize_t (*write)(struct transport *, const void *buf, size_t size);
};

struct transport
{
	int   error;
	int   type;
	void *private_data;
	struct transport_operations *t_ops;
};

#ifdef __cplusplus
extern "C" {
#endif

int     transport_destroy(struct transport *t);
ssize_t transport_read(struct transport *t, void *buf, size_t size);
int     transport_read_all(struct transport *t, void *buf, size_t size);
ssize_t transport_write(struct transport *t, const void *buf, size_t size);
int     transport_write_all(struct transport *t, const void *buf, size_t size);

#ifdef __cplusplus
};
#endif

#endif
