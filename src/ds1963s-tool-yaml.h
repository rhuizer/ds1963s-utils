/* ds1963s-tool-yaml.h
 *
 * Emit a yaml configuration defining the ds1963s ibutton dumped.
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
#ifndef DS1963S_TOOL_YAML_H
#define DS1963S_TOOL_YAML_H

#include <yaml.h>
#include "ds1963s-tool.h"

#ifdef __cplusplus
extern "C" {
#endif

void ds1963s_tool_rom_print_yaml(struct ds1963s_tool *tool, struct ds1963s_rom *rom);
void ds1963s_tool_write_cycle_counters_print_yaml(struct ds1963s_tool *tool, uint32_t counters[16]);
void ds1963s_tool_info_yaml(struct ds1963s_tool *tool);
void ds1963s_tool_info_full_yaml(struct ds1963s_tool *tool);

#ifdef __cplusplus
};
#endif

#endif
