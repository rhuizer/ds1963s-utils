/* transport.c
 *
 * Transport layer base class.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 *  -- Ronald Huizer, (C) 2016
 */
#include <assert.h>
#include "transport.h"

int transport_destroy(struct transport *t)
{
	if (t->t_ops->destroy == NULL) {
		t->error = TRANSPORT_ERROR_UNSUPPORTED;
		return -1;
	}

	return t->t_ops->destroy(t);
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
