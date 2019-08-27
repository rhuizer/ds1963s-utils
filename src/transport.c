/* transport.c
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
#include <assert.h>
#include <stdlib.h>
#include "transport.h"

int transport_destroy(struct transport *t)
{
	if (t->t_ops->destroy == NULL) {
		t->error = TRANSPORT_ERROR_UNSUPPORTED;
		return -1;
	}

	if (t->t_ops->destroy(t) == -1) {
		t->error = TRANSPORT_ERROR_DESTROY;
		return -1;
	}

	free(t);
	return 0;
}

ssize_t transport_read(struct transport *t, void *buf, size_t size)
{
	assert(t != NULL);

	if (t->t_ops->read == NULL) {
		t->error = TRANSPORT_ERROR_UNSUPPORTED;
		return -1;
	}

	return t->t_ops->read(t, buf, size);
}

int transport_read_all(struct transport *t, void *buf, size_t size)
{
	size_t total = 0;
	ssize_t ret;

	if (size == 0)
		return 0;

	do {
		ret = transport_read(t, buf + total, size - total);
		if (ret <= 0)
			return -1;

		total += ret;
	} while (total != size);

	return 0;
}

ssize_t transport_write(struct transport *t, const void *buf, size_t size)
{
	assert(t != NULL);

	if (t->t_ops->write == NULL) {
		t->error = TRANSPORT_ERROR_UNSUPPORTED;
		return -1;
	}

	return t->t_ops->write(t, buf, size);
}

int transport_write_all(struct transport *t, const void *buf, size_t size)
{
	size_t total = 0;
	ssize_t ret;

	if (size == 0)
		return 0;

	do {
		ret = transport_write(t, buf + total, size - total);
		if (ret <= 0)
			return -1;

		total += ret;
	} while (total != size);

	return 0;
}
