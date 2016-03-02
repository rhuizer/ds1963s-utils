#ifndef __TRANSPORT_FACTORY_H
#define __TRANSPORT_FACTORY_H

#include "transport.h"

#define TRANSPORT_UNIX	1

#ifdef __cplusplus
extern "C" {
#endif

struct transport *transport_factory_new(int type);

#ifdef __cplusplus
};
#endif

#endif
