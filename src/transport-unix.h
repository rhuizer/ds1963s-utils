#ifndef __TRANSPORT_UNIX_H
#define __TRANSPORT_UNIX_H

#include <stddef.h>
#include "transport.h"

struct transport_unix_data
{
	int	sd;
};

#ifdef __cplusplus
extern "C" {
#endif

int               transport_unix_init(struct transport *t);
struct transport *transport_unix_new(void);
int               transport_unix_bind(struct transport *t, const char *pathname);
int               transport_unix_connect(struct transport *t, const char *pathname);

#ifdef __cplusplus
};
#endif

#endif
