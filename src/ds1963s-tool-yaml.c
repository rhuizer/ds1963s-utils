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
__yaml_add_int(yaml_emitter_t *emitter, const char *value)
{
	yaml_event_t event;

	yaml_scalar_event_initialize(
		&event,
		NULL,
		(yaml_char_t *)YAML_INT_TAG,
		(yaml_char_t *)value,
		strlen(value),
		1,
		0,
		YAML_PLAIN_SCALAR_STYLE
	);

        if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);
}

static void
__yaml_add_string(yaml_emitter_t *emitter, const char *name)
{
	yaml_event_t event;

	yaml_scalar_event_initialize(
		&event,
		NULL,
		(yaml_char_t *)YAML_STR_TAG,
		(yaml_char_t *)name,
		strlen(name),
		1,
		0,
		YAML_PLAIN_SCALAR_STYLE
	);

	if (!yaml_emitter_emit(emitter, &event))
		__yaml_emitter_error(emitter, &event);
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
		snprintf(&buf[i * 2], sizeof(buf) - i * 2, "%.2x",
		         rom->serial[sizeof(rom->serial) - i - 1]);
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
	char buf[32];

	__yaml_add_string(emitter, name);
	snprintf(buf, sizeof buf, "0x%.8x", value);
	__yaml_add_int(emitter, buf);
}

void
ds1963s_tool_write_cycle_counters_print_yaml(struct ds1963s_tool *tool, uint32_t counters[16])
{
	__yaml_add_string(&tool->emitter, "write-cycle-counters");
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

                snprintf(buf, sizeof buf, "page_%.2d", i);
		__yaml_add_string(&tool->emitter, buf);

                for (j = 0; j < sizeof page; j++)
                        snprintf(buf + j * 2, sizeof(buf) - j * 2, "%.2x", page[j]);
		__yaml_add_string(&tool->emitter, buf);
        }

	__yaml_end_map(&tool->emitter);
}

void ds1963s_tool_info_yaml(struct ds1963s_tool *tool)
{
	struct ds1963s_rom rom;
	uint32_t counters[16];
	yaml_event_t event;
	char buf[16];

	yaml_emitter_initialize(&tool->emitter);
	yaml_emitter_set_output_file(&tool->emitter, stdout);

	yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
	if (!yaml_emitter_emit(&tool->emitter, &event))
		__yaml_emitter_error(&tool->emitter, &event);

	yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
	if (!yaml_emitter_emit(&tool->emitter, &event))
		__yaml_emitter_error(&tool->emitter, &event);

	__yaml_start_map(&tool->emitter);

	if (ds1963s_client_rom_get(&tool->client, &rom) != -1)
		ds1963s_tool_rom_print_yaml(tool, &rom);

	if (ds1963s_write_cycle_get_all(&tool->client, counters) != -1)
		ds1963s_tool_write_cycle_counters_print_yaml(tool, counters);

	ds1963s_tool_memory_dump_yaml(tool);

	__yaml_add_string(&tool->emitter, "prng_counter");
	snprintf(buf, sizeof buf, "0x%.8x",
		ds1963s_client_prng_get(&tool->client));
	__yaml_add_int(&tool->emitter, buf);

	__yaml_end_map(&tool->emitter);

	yaml_document_end_event_initialize(&event, 1);
	if (!yaml_emitter_emit(&tool->emitter, &event))
		__yaml_emitter_error(&tool->emitter, &event);

	yaml_stream_end_event_initialize(&event);
	if (!yaml_emitter_emit(&tool->emitter, &event))
		__yaml_emitter_error(&tool->emitter, &event);

	yaml_emitter_delete(&tool->emitter);
}
