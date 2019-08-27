/* transport-pty.c
 *
 * Transport layer pty implementation.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * Copyright (C) 2019  Ronald Huizer <rhuizer@hexpedition.com>
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
#define _XOPEN_SOURCE 600
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "transport-pty.h"

static struct transport_operations transport_pty_operations;

static int
close_no_EINTR(int fd)
{
	int ret;

	do {
		ret = close(fd);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

static ssize_t
read_no_EINTR(int fd, void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = read(fd, buf, count);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

static ssize_t
write_no_EINTR(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = write(fd, buf, count);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

int
transport_pty_init(struct transport *t)
{
	struct transport_pty_data *data;

	assert(t != NULL);

	if ( (data = malloc(sizeof *data)) == NULL)
		goto err;

	if ( (data->fd = posix_openpt(O_RDWR | O_NOCTTY)) == -1)
		goto err_free;

	if (grantpt(data->fd) == -1)
		goto err_fd;

	if (unlockpt(data->fd) == -1)
		goto err_fd;

	if ( (data->pathname_slave = ptsname(data->fd)) == NULL)
		goto err_fd;

	if ( (data->pathname_slave = strdup(data->pathname_slave)) == NULL)
		goto err_fd;

	t->t_ops        = &transport_pty_operations;
	t->private_data = data;

	return 0;

err_fd:
	close_no_EINTR(data->fd);
err_free:
	free(data);
err:
	return -1;
}

struct transport *transport_pty_new(void)
{
	struct transport *t;

	if ( (t = malloc(sizeof *t)) == NULL)
		return NULL;

	if (transport_pty_init(t) == -1) {
		free(t);
		return NULL;
	}

	return t;
}

static int
transport_pty_destroy(struct transport *t)
{
	struct transport_pty_data *data;

	assert(t != NULL);
	assert(t->private_data != NULL);

	data = t->private_data;
	close_no_EINTR(data->fd);
	free(data->pathname_slave);
	free(data);

	return 0;
}

static ssize_t
transport_pty_read(struct transport *t, void *buf, size_t count)
{
	struct transport_pty_data *data;

	assert(t != NULL);
	assert(t->private_data != NULL);

	data = t->private_data;
	return read_no_EINTR(data->fd, buf, count);
}

static ssize_t
transport_pty_write(struct transport *t, const void *buf, size_t count)
{
	struct transport_pty_data *data;

	assert(t != NULL);
	assert(t->private_data != NULL);

	data = t->private_data;
	return write_no_EINTR(data->fd, buf, count);
}

static struct transport_operations transport_pty_operations = {
	.destroy = transport_pty_destroy,
	.read	 = transport_pty_read,
	.write	 = transport_pty_write
};
