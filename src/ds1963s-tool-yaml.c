/* ds1963s-tool-yaml.c
 *
 * Emit a yaml configuration defining the ds1963s ibutton dumped.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * Not a fan of libyaml in C, this code is a horribly convoluted state machine.
 * Either I've gotten the use wrong, or the design of libyaml is bad.  Instead
 * of saving time and work it wasted and created it, which is the opposite of
 * what a good library is supposed to do.
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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <yaml.h>
#include "ds1963s-tool.h"
#include "ds1963s-tool-yaml.h"

static void
__yaml_emitter_error(yaml_emitter_t *emitter, yaml_event_t *event)
{
	fprintf(stderr, "Failed to emit event %d: %s\n", event->type, emitter->problem);
	yaml_emitter_delete(emitter);
	exit(EXIT_FAILURE);
}

static void
__yaml_start(yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_emitter_initialize(emitter);
	yaml_emitter_set_output_file(emitter, stdout);

	yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
	if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);

	yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
	if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);
}

static void
__yaml_end(yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_document_end_event_initialize(&event, 1);
	if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);

	yaml_stream_end_event_initialize(&event);
	if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);

	yaml_emitter_delete(emitter);
}

static void
__yaml_start_map(yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_mapping_start_event_initialize(
		&event,
		NULL,
		(yaml_char_t *)YAML_MAP_TAG,
		1,
		YAML_ANY_MAPPING_STYLE
	);

	if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);
}

static void
__yaml_end_map(yaml_emitter_t *emitter)
{
	yaml_event_t event;

	yaml_mapping_end_event_initialize(&event);

        if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);
}

static void
__yaml_add_tag(yaml_emitter_t *emitter, char *tag, const char *fmt, va_list ap)
{
	yaml_event_t event;
	char *s;

	if (vasprintf(&s, fmt, ap) == -1) {
		perror("vasprintf()");
		exit(EXIT_FAILURE);
	}

	yaml_scalar_event_initialize(
		&event,
		NULL,
		(yaml_char_t *)tag,
		(yaml_char_t *)s,
		strlen(s),
		1,
		0,
		YAML_PLAIN_SCALAR_STYLE
	);

	if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);
}

static void
__yaml_add_int(yaml_emitter_t *emitter, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	__yaml_add_tag(emitter, YAML_INT_TAG, fmt, ap);
	va_end(ap);
}

static void
__yaml_add_string(yaml_emitter_t *emitter, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	__yaml_add_tag(emitter, YAML_STR_TAG, fmt, ap);
	va_end(ap);
}

void
ds1963s_tool_rom_print_yaml(struct ds1963s_tool *tool, struct ds1963s_rom *rom)
{
	char buf[32];

	assert(tool != NULL);
	assert(rom != NULL);

	/* rom family */
	__yaml_add_string(&tool->emitter, "family");

	snprintf(buf, sizeof buf, "0x%.2x", rom->family);
	__yaml_add_int(&tool->emitter, buf);

	/* rom serial */
	__yaml_add_string(&tool->emitter, "serial");

	for (int i = 0; i < sizeof(rom->serial); i++) {
		snprintf(&buf[i * 2], sizeof(buf) - i * 2,
		         "%.2x", rom->serial[i]);
	}
	__yaml_add_string(&tool->emitter, buf);

	/* rom crc8 */
	__yaml_add_string(&tool->emitter, "crc8");
	snprintf(buf, sizeof buf, "0x%.2x", rom->crc);
	__yaml_add_int(&tool->emitter, buf);
}

static void
__write_cycle_print_yaml(yaml_emitter_t *emitter, const char *name, uint32_t value)
{
	__yaml_add_string(emitter, name);
	__yaml_add_int(emitter, "0x%.8x", value);
}

void
ds1963s_tool_write_cycle_counters_print_yaml(struct ds1963s_tool *tool, uint32_t counters[16])
{
	__yaml_add_string(&tool->emitter, "write_cycle_counters");
	__yaml_start_map(&tool->emitter);
	__write_cycle_print_yaml(&tool->emitter, "data_page_08", counters[0]);
	__write_cycle_print_yaml(&tool->emitter, "data_page_09", counters[1]);
	__write_cycle_print_yaml(&tool->emitter, "data_page_10", counters[2]);
	__write_cycle_print_yaml(&tool->emitter, "data_page_11", counters[3]);
	__write_cycle_print_yaml(&tool->emitter, "data_page_12", counters[4]);
	__write_cycle_print_yaml(&tool->emitter, "data_page_13", counters[5]);
	__write_cycle_print_yaml(&tool->emitter, "data_page_14", counters[6]);
	__write_cycle_print_yaml(&tool->emitter, "data_page_15", counters[7]);
	__write_cycle_print_yaml(&tool->emitter, "secret_0", counters[8]);
	__write_cycle_print_yaml(&tool->emitter, "secret_1", counters[9]);
	__write_cycle_print_yaml(&tool->emitter, "secret_2", counters[10]);
	__write_cycle_print_yaml(&tool->emitter, "secret_3", counters[11]);
	__write_cycle_print_yaml(&tool->emitter, "secret_4", counters[12]);
	__write_cycle_print_yaml(&tool->emitter, "secret_5", counters[13]);
	__write_cycle_print_yaml(&tool->emitter, "secret_6", counters[14]);
	__write_cycle_print_yaml(&tool->emitter, "secret_7", counters[15]);
	__yaml_end_map(&tool->emitter);
}

void ds1963s_tool_memory_dump_yaml(struct ds1963s_tool *tool)
{
        struct ds1963s_client *ctx = &tool->client;
        uint8_t page[32];
	char buf[128];
        int i, j;

	__yaml_add_string(&tool->emitter, "nvram");
	__yaml_start_map(&tool->emitter);

        for (i = 0; i < 16; i++) {
                if (ds1963s_client_memory_read(ctx, 32 * i, page, 32) == -1)
                        continue;

		__yaml_add_string(&tool->emitter, "page_%.2d", i);

                for (j = 0; j < sizeof page; j++)
                        snprintf(buf + j * 2, sizeof(buf) - j * 2, "%.2x", page[j]);
		__yaml_add_string(&tool->emitter, buf);
        }

	__yaml_end_map(&tool->emitter);
}

static void
__info_yaml(struct ds1963s_tool *tool, struct ds1963s_rom *rom,
            uint32_t counters[16])
{
	uint32_t prng;

	ds1963s_client_rom_get(&tool->client, rom);
	ds1963s_tool_rom_print_yaml(tool, rom);

	if (ds1963s_write_cycle_get_all(&tool->client, counters) != -1)
		ds1963s_tool_write_cycle_counters_print_yaml(tool, counters);

	ds1963s_tool_memory_dump_yaml(tool);

	__yaml_add_string(&tool->emitter, "prng_counter");
	prng = ds1963s_client_prng_get(&tool->client);
	__yaml_add_int(&tool->emitter, "0x%.8x", prng);
}

void
ds1963s_tool_info_yaml(struct ds1963s_tool *tool)
{
	struct ds1963s_rom rom;
	uint32_t counters[16];

	__yaml_start(&tool->emitter);
	__yaml_start_map(&tool->emitter);
	__info_yaml(tool, &rom, counters);
	__yaml_end_map(&tool->emitter);
	__yaml_end(&tool->emitter);
}

void
ds1963s_tool_info_full_yaml(struct ds1963s_tool *tool)
{
	struct ds1963s_rom rom;
	uint32_t counters[16];
	char buf[128];

	__yaml_start(&tool->emitter);
	__yaml_start_map(&tool->emitter);
	__info_yaml(tool, &rom, counters);

	ds1963s_tool_secrets_get(tool, &rom, counters);

	__yaml_add_string(&tool->emitter, "secrets");
	__yaml_start_map(&tool->emitter);
        for (int secret = 0; secret < 8; secret++) {
		__yaml_add_string(&tool->emitter, "secret_%d", secret);
                for (int i = 0; i < 8; i++) {
			sprintf(buf + i * 2, "%.2x",
			        tool->brute.secrets[secret].secret[i]);
		}
		__yaml_add_string(&tool->emitter, buf);
        }
	__yaml_end_map(&tool->emitter);
	__yaml_end_map(&tool->emitter);
	__yaml_end(&tool->emitter);
}
