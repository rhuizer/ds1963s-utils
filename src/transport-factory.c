/* transport-factory.c
 *
 * A factory for transport layer instances.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 *  -- Ronald Huizer, (C) 2016
 */
#include "transport-factory.h"
#include "transport-unix.h"

struct transport *transport_factory_new(int type)
{
	switch (type) {
	case TRANSPORT_UNIX:
		return transport_unix_new();
	}

	return NULL;
}
