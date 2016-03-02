#ifndef __DS2480_DEVICE_H
#define __DS2480_DEVICE_H

#include "transport.h"

struct ds2480_device
{
	int	state;
};

#ifdef __cplusplus
extern "C" {
#endif

int ds2480_dev_detect_handle(struct ds2480_device *dev, struct transport *t);

#ifdef __cplusplus
};
#endif

#endif
