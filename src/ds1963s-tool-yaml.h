#ifndef __DS1963S_TOOL_YAML_H
#define __DS1963S_TOOL_YAML_H

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
