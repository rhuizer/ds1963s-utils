/* transport-pty.h
 *
 * Transport layer UNIX socket implementation.
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
#ifndef TRANSPORT_PTY_H
#define TRANSPORT_PTY_H

#include <stddef.h>
#include "transport.h"

struct transport_pty_data
{
	int	fd;
	char *	pathname_slave;
};

#ifdef __cplusplus
extern "C" {
#endif

int               transport_pty_init(struct transport *t);
struct transport *transport_pty_new(void);

#ifdef __cplusplus
};
#endif

#endif
