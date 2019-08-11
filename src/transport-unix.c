/* transport-unix.c
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
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "transport-unix.h"

static struct transport_operations transport_unix_operations;

static int close_no_EINTR(int fd)
{
	int ret;

	do {
		ret = close(fd);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

static ssize_t read_no_EINTR(int fd, void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = read(fd, buf, count);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

static ssize_t write_no_EINTR(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = write(fd, buf, count);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

int transport_unix_init(struct transport *t)
{
	struct transport_unix_data *data;

	assert(t != NULL);

	if ( (data = malloc(sizeof *data)) == NULL)
		return -1;

	if ( (data->sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		free(data);
		return -1;
	}

	t->t_ops        = &transport_unix_operations;
	t->private_data = data;

	return 0;
}

struct transport *transport_unix_new(void)
{
	struct transport *t;

	if ( (t = malloc(sizeof *t)) == NULL)
		return NULL;

	if (transport_unix_init(t) == -1) {
		free(t);
		return NULL;
	}

	return t;
}

static int transport_unix_destroy(struct transport *t)
{
	struct transport_unix_data *data;

	assert(t != NULL);
	assert(t->private_data != NULL);

	data = t->private_data;
	close_no_EINTR(data->sd);
	free(data);

	return 0;
}

int transport_unix_bind(struct transport *t, const char *pathname)
{
	struct transport_unix_data *data;
	struct sockaddr_un sun;

	assert(t != NULL);
	assert(t->private_data != NULL);

	data = t->private_data;
	memset(&sun, 0, sizeof sun);
	sun.sun_family = AF_UNIX;

	/* If pathname is NULL, we use auto-binding. */
	if (pathname == NULL)
		return bind(data->sd, (struct sockaddr *)&sun, sizeof(sa_family_t));

	/* No auto-binding requested; bind the pathname. */
	if (strlen(pathname) >= sizeof(sun.sun_path))
		return -1;
	strcpy(sun.sun_path, pathname);

	return bind(data->sd, (struct sockaddr *)&sun, sizeof sun);
}

int transport_unix_connect(struct transport *t, const char *pathname)
{
	struct transport_unix_data *data;
	struct sockaddr_un sun;

	assert(t != NULL);
	assert(t->private_data != NULL);
	assert(pathname != NULL);

	data = t->private_data;

	if (strlen(pathname) >= sizeof(sun.sun_path)) {
		errno = EOVERFLOW;
		return -1;
	}

	memset(&sun, 0, sizeof sun);
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, pathname);

	return connect(data->sd, (struct sockaddr *)&sun, sizeof sun);
}

static ssize_t transport_unix_read(struct transport *t, void *buf, size_t count)
{
	struct transport_unix_data *data;

	assert(t != NULL);
	assert(t->private_data != NULL);

	data = t->private_data;
	return read_no_EINTR(data->sd, buf, count);
}

static ssize_t
transport_unix_write(struct transport *t, const void *buf, size_t count)
{
	struct transport_unix_data *data;

	assert(t != NULL);
	assert(t->private_data != NULL);

	data = t->private_data;
	return write_no_EINTR(data->sd, buf, count);
}

static struct transport_operations transport_unix_operations = {
	.destroy = transport_unix_destroy,
	.read	 = transport_unix_read,
	.write	 = transport_unix_write
};
