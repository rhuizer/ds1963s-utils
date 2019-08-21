#ifndef __DS1963S_EMULATOR_YAML
#define __DS1963S_EMULATOR_YAML

#include "ds1963s-device.h"

#ifdef __cplusplus
extern "C" {
#endif

int ds1963s_emulator_yaml_load(struct ds1963s_device *dev, const char *pathname);

#ifdef __cplusplus
};
#endif

#endif
