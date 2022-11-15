/* ds1963s-emulator-yaml.h
 *
 * Parse a yaml configuration defining the ds1963s ibutton to emulate.
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
#ifndef DS1963S_EMULATOR_YAML
#define DS1963S_EMULATOR_YAML

#include "ds1963s-device.h"

#ifdef __cplusplus
extern "C" {
#endif

int ds1963s_emulator_yaml_load(struct ds1963s_device *dev, const char *pathname);

#ifdef __cplusplus
};
#endif

#endif
