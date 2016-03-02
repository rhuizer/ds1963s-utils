#ifndef __TRANSPORT_H
#define __TRANSPORT_H

#include <stdio.h>

#define TRANSPORT_ERROR_NONE		0
#define TRANSPORT_ERROR_UNSUPPORTED	1

struct transport;

struct transport_operations
{
	int     (*destroy)(struct transport *);
	ssize_t (*read)(struct transport *, void *buf, size_t size);
	ssize_t (*write)(struct transport *, const void *buf, size_t size);
};

struct transport
{
	int error;
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
