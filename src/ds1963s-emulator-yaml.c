/* ds1963s-emulator-yaml.c
 *
 * Parse a yaml configuration defining the ds1963s ibutton to emulate.
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
#include <ctype.h>
#include <yaml.h>
#include "ds1963s-common.h"
#include "ds1963s-device.h"

#define STATE_INIT		0
#define STATE_DOCUMENT		1
#define STATE_GLOBAL_MAP	2
#define STATE_KEY		3
#define STATE_FAMILY		4
#define STATE_SERIAL		5
#define STATE_CRC8		6
#define STATE_PRNG_COUNTER	7
#define STATE_WRITE_CYCLE_MAP	8
#define STATE_WRITE_CYCLE_KEY	9
#define STATE_WRITE_CYCLE_VALUE	10
#define STATE_NVRAM_MAP		11
#define STATE_NVRAM_KEY		12
#define STATE_NVRAM_VALUE	13
#define STATE_SECRET_MAP	14
#define STATE_SECRET_KEY	15
#define STATE_SECRET_VALUE	16
#define STATE_DOCUMENT_END	17
#define STATE_STREAM_END	18

#define SEEN_FAMILY		1
#define SEEN_SERIAL		2
#define SEEN_CRC8		4
#define SEEN_PRNG_COUNTER	8
#define SEEN_SECRET		16
#define SEEN_WRITE_CYCLE	32
#define SEEN_NVRAM		64
#define SEEN_ALL		127

struct parser_context {
	yaml_parser_t	parser;
	yaml_event_t	event;
	int		state;
	uint16_t	key_seen;

	uint8_t		family;
	uint8_t		serial[6];

	uint32_t	prng_counter;

	uint32_t *	wcp;
	uint16_t	wc_seen;
	uint32_t	data_wc[8];
	uint32_t	secret_wc[8];

	uint8_t *	nvp;
	uint16_t	nvram_seen;
	uint8_t		nvram[512];

	uint8_t *	sp;
	uint8_t		secret_seen;
	uint8_t		secret[64];

};

static void
__parser_init(struct parser_context *ctx, FILE *fp)
{
	memset(ctx, 0, sizeof *ctx);
	yaml_parser_initialize(&ctx->parser);
	yaml_parser_set_input_file(&ctx->parser, fp);
	ctx->state    = STATE_INIT;
}

static void
__parser_error(struct parser_context *ctx)
{
	assert(ctx != NULL);

        fprintf(stderr, "Parse error %d: %s\n", ctx->parser.error,
	                                        ctx->parser.problem);
        yaml_parser_delete(&ctx->parser);
        exit(EXIT_FAILURE);
}

static void
__parser_parse(struct parser_context *ctx)
{
	if (yaml_parser_parse(&ctx->parser, &ctx->event) == -1)
		__parser_error(ctx);
}

static void
__yaml_key_error(yaml_parser_t *parser, const char *key)
{
	fprintf(stderr, "Parse error: unknown key `%s'.\n", key);
	yaml_parser_delete(parser);
	exit(EXIT_FAILURE);
}

static unsigned int
__parse_int(yaml_parser_t *parser, const char *s)
{
	char *endptr;
	int ret;

	if (*s == 0) {
		fprintf(stderr, "Parse error: empty value.\n");
		yaml_parser_delete(parser);
		exit(EXIT_FAILURE);
	}

	ret = strtoul(s, &endptr, 16);

	if (*endptr != 0) {
		fprintf(stderr, "Parse error: invalid integer: `%s'\n", endptr);
		yaml_parser_delete(parser);
		exit(EXIT_FAILURE);
	}

	return ret;
}

static void
__is_hex_string(yaml_parser_t *parser, const char *s)
{
	for (; *s != 0; s++) {
		if (!isxdigit(*s)) {
			fprintf(stderr, "Parse error: invalid hex string: `%s'\n", s);
			yaml_parser_delete(parser);
			exit(EXIT_FAILURE);
		}
	}
}

static void
__is_length(yaml_parser_t *parser, const char *s, size_t size)
{
	if (strlen(s) != size) {
		fprintf(stderr, "Parse error: invalid length: `%s'. "
		                "Expected %zu.\n", s, size);
		yaml_parser_delete(parser);
		exit(EXIT_FAILURE);
	}
}

static void
__handle_init(struct parser_context *ctx)
{
	assert(ctx != NULL);
	assert(ctx->state == STATE_INIT);

	if (ctx->event.type != YAML_STREAM_START_EVENT) {
		fprintf(stderr, "Expected start event.\n");
		exit(EXIT_FAILURE);
	}

	ctx->state = STATE_DOCUMENT;
}

static void
__handle_stream_end(struct parser_context *ctx)
{
	assert(ctx != NULL);
	assert(ctx->state == STATE_STREAM_END);

	if (ctx->event.type != YAML_STREAM_END_EVENT) {
		fprintf(stderr, "Expected stream end event.\n");
		exit(EXIT_FAILURE);
	}
}

static void
__handle_global_map(struct parser_context *ctx)
{
	assert(ctx != NULL);
	assert(ctx->state == STATE_GLOBAL_MAP);

	if (ctx->event.type != YAML_MAPPING_START_EVENT) {
		fprintf(stderr, "Expected map start event.\n");
		exit(EXIT_FAILURE);
	}

	ctx->state = STATE_KEY;
}

static void
__handle_global_key(struct parser_context *ctx)
{
	const char *str;

	if (ctx->event.type == YAML_MAPPING_END_EVENT) {
		if (ctx->key_seen != SEEN_ALL) {
			fprintf(stderr, "Warning: not all keys have been "
			                "defined.\n");
		}

		ctx->state = STATE_DOCUMENT_END;
		return;
	}

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	str = (char *)ctx->event.data.scalar.value;

	if (strcmp(str, "family") == 0)
		ctx->state = STATE_FAMILY;
	else if (strcmp(str, "serial") == 0)
		ctx->state = STATE_SERIAL;
	else if (strcmp(str, "crc8") == 0)
		ctx->state = STATE_CRC8;
	else if (strcmp(str, "write_cycle_counters") == 0)
		ctx->state = STATE_WRITE_CYCLE_MAP;
	else if (strcmp(str, "nvram") == 0)
		ctx->state = STATE_NVRAM_MAP;
	else if (strcmp(str, "prng_counter") == 0)
		ctx->state = STATE_PRNG_COUNTER;
	else if (strcmp(str, "secrets") == 0)
		ctx->state = STATE_SECRET_MAP;
	else
		__yaml_key_error(&ctx->parser, str);
}

static void
__handle_family(struct parser_context *ctx)
{
	const char *str;

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	str = (char *)ctx->event.data.scalar.value;

	ctx->family    = __parse_int(&ctx->parser, str);
	ctx->state     = STATE_KEY;
	ctx->key_seen |= SEEN_FAMILY;
}

static void
__handle_serial(struct parser_context *ctx)
{
	const char *str;

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	str = (char *)ctx->event.data.scalar.value;

	__is_hex_string(&ctx->parser, str);
	__is_length(&ctx->parser, str, sizeof(ctx->serial) * 2);
	hex_decode(ctx->serial, str, sizeof ctx->serial);
	ctx->state     = STATE_KEY;
	ctx->key_seen |= SEEN_SERIAL;
}

static void
__handle_prng(struct parser_context *ctx)
{
	const char *str;

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	str = (char *)ctx->event.data.scalar.value;

	ctx->prng_counter = __parse_int(&ctx->parser, str);
	ctx->state        = STATE_KEY;
	ctx->key_seen    |= SEEN_PRNG_COUNTER;
}

static void
__handle_document(struct parser_context *ctx)
{
	assert(ctx != NULL);
	assert(ctx->state == STATE_DOCUMENT);

	if (ctx->event.type != YAML_DOCUMENT_START_EVENT) {
		fprintf(stderr, "Expected start event.\n");
		exit(EXIT_FAILURE);
	}

	ctx->state = STATE_GLOBAL_MAP;
}

static void
__handle_document_end(struct parser_context *ctx)
{
	assert(ctx != NULL);
	assert(ctx->state == STATE_DOCUMENT_END);

	if (ctx->event.type != YAML_DOCUMENT_END_EVENT) {
		fprintf(stderr, "Expected document end event.\n");
		exit(EXIT_FAILURE);
	}

	ctx->state = STATE_STREAM_END;
}

static void
__handle_write_cycle_map(struct parser_context *ctx)
{
	assert(ctx != NULL);
	assert(ctx->state == STATE_WRITE_CYCLE_MAP);

	if (ctx->event.type != YAML_MAPPING_START_EVENT) {
		fprintf(stderr, "Expected start event.\n");
		exit(EXIT_FAILURE);
	}

	ctx->state = STATE_WRITE_CYCLE_KEY;
}

static void
__handle_write_cycle_key(struct parser_context *ctx)
{
	unsigned int i;
	const char *s;
	char *endptr;

	assert(ctx != NULL);
	assert(ctx->state == STATE_WRITE_CYCLE_KEY);

	if (ctx->event.type == YAML_MAPPING_END_EVENT) {
		if (ctx->wc_seen != 0xFFFF) {
			fprintf(stderr, "Warning: not all write cycle "
					"counters have been defined.\n");
		}

		ctx->state = STATE_KEY;
		return;
	}

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	s = (char *)ctx->event.data.scalar.value;

	if (!strncmp(s, "data_page_", 10) && s[10] != 0) {
		i = strtoul(&s[10], &endptr, 10);
		if (*endptr != 0 || i < 8 || i > 15)
			goto error;

		if (ctx->wc_seen & (i << i)) {
			fprintf(stderr, "Warning: redefined write cycle "
					"counter `%s'.\n", s);
		}

		ctx->wc_seen |= 1 << i;
		ctx->wcp      = &ctx->data_wc[i - 8];
		ctx->state    = STATE_WRITE_CYCLE_VALUE;
		return;
	}

	if (!strncmp(s, "secret_", 7) && s[7] != 0) {
		i = strtoul(&s[7], &endptr, 10);
		if (*endptr != 0 || i > 7)
			goto error;

		ctx->wc_seen |= 1 << i;
		ctx->wcp      = &ctx->secret_wc[i];
		ctx->state    = STATE_WRITE_CYCLE_VALUE;
		return;
	}

error:
	fprintf(stderr, "Parse error: unknown write-cycle key `%s'.\n", s);
	yaml_parser_delete(&ctx->parser);
	exit(EXIT_FAILURE);
}

static void
__handle_write_cycle_value(struct parser_context *ctx)
{
	const char *str;

	assert(ctx != NULL);
	assert(ctx->state == STATE_WRITE_CYCLE_VALUE);

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	str       = (char *)ctx->event.data.scalar.value;
	*ctx->wcp = __parse_int(&ctx->parser, str);

	ctx->state = STATE_WRITE_CYCLE_KEY;
}

static void
__handle_nvram_map(struct parser_context *ctx)
{
	assert(ctx != NULL);
	assert(ctx->state == STATE_NVRAM_MAP);

	if (ctx->event.type != YAML_MAPPING_START_EVENT) {
		fprintf(stderr, "Expected start event.\n");
		exit(EXIT_FAILURE);
	}

	ctx->state = STATE_NVRAM_KEY;
}

static void
__handle_nvram_key(struct parser_context *ctx)
{
	unsigned int i;
	const char *s;
	char *endptr;

	assert(ctx != NULL);
	assert(ctx->state == STATE_NVRAM_KEY);

	if (ctx->event.type == YAML_MAPPING_END_EVENT) {
		if (ctx->nvram_seen != 0xFFFF) {
			fprintf(stderr, "Warning: not all nvram pages "
					"have been defined.\n");
		}

		ctx->state = STATE_KEY;
		return;
	}

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	s = (char *)ctx->event.data.scalar.value;

	if (!strncmp(s, "page_", 5) && s[5] != 0) {
		i = strtoul(&s[5], &endptr, 10);
		if (*endptr != 0 || i > 15)
			goto error;

		if (ctx->nvram_seen & (i << i)) {
			fprintf(stderr, "Warning: redefined nvram "
					"page `%s'.\n", s);
		}

		ctx->nvram_seen |= 1 << i;
		ctx->nvp         = &ctx->nvram[i * 32];
		ctx->state       = STATE_NVRAM_VALUE;
		return;
	}

error:
	fprintf(stderr, "Parse error: unknown nvram key `%s'.\n", s);
	yaml_parser_delete(&ctx->parser);
	exit(EXIT_FAILURE);
}

static void
__handle_nvram_value(struct parser_context *ctx)
{
	const char *str;

	assert(ctx != NULL);
	assert(ctx->state == STATE_NVRAM_VALUE);

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	str = (char *)ctx->event.data.scalar.value;
	__is_hex_string(&ctx->parser, str);
	__is_length(&ctx->parser, str, 64);
	hex_decode(ctx->nvp, str, 32);
	ctx->state = STATE_NVRAM_KEY;
}

static void
__handle_secret_map(struct parser_context *ctx)
{
	assert(ctx != NULL);
	assert(ctx->state == STATE_SECRET_MAP);

	if (ctx->event.type != YAML_MAPPING_START_EVENT) {
		fprintf(stderr, "Expected start event.\n");
		exit(EXIT_FAILURE);
	}

	ctx->state = STATE_SECRET_KEY;
}

static void
__handle_secret_key(struct parser_context *ctx)
{
	unsigned int i;
	const char *s;
	char *endptr;

	assert(ctx != NULL);
	assert(ctx->state == STATE_SECRET_KEY);

	if (ctx->event.type == YAML_MAPPING_END_EVENT) {
		if (ctx->secret_seen != 0xFF) {
			fprintf(stderr, "Warning: not all secrets "
					"have been defined.\n");
		}

		ctx->state = STATE_KEY;
		return;
	}

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	s = (char *)ctx->event.data.scalar.value;

	if (!strncmp(s, "secret_", 7) && s[7] != 0) {
		i = strtoul(&s[7], &endptr, 10);
		if (*endptr != 0 || i > 7)
			goto error;

		if (ctx->secret_seen & (i << i)) {
			fprintf(stderr, "Warning: redefined secret "
					"`%s'.\n", s);
		}

		ctx->secret_seen |= 1 << i;
		ctx->sp           = &ctx->secret[i * 8];
		ctx->state        = STATE_SECRET_VALUE;
		return;
	}

error:
	fprintf(stderr, "Parse error: unknown nvram key `%s'.\n", s);
	yaml_parser_delete(&ctx->parser);
	exit(EXIT_FAILURE);
}

static void
__handle_secret_value(struct parser_context *ctx)
{
	const char *str;

	assert(ctx != NULL);
	assert(ctx->state == STATE_SECRET_VALUE);

	if (ctx->event.type != YAML_SCALAR_EVENT) {
		fprintf(stderr, "Expected scalar event.\n");
		exit(EXIT_FAILURE);
	}

	str = (char *)ctx->event.data.scalar.value;
	__is_hex_string(&ctx->parser, str);
	__is_length(&ctx->parser, str, 16);
	hex_decode(ctx->sp, str, 8);
	ctx->state = STATE_SECRET_KEY;
}

int
ds1963s_emulator_yaml_load(struct ds1963s_device *dev, const char *pathname)
{
	struct parser_context ctx;
	FILE *fp;

	if ( (fp = fopen(pathname, "r")) == NULL) {
		perror("fopen()");
		exit(EXIT_FAILURE);
	}

	__parser_init(&ctx, fp);

	do {
		__parser_parse(&ctx);

		switch (ctx.state) {
		case STATE_INIT:
			__handle_init(&ctx);
			break;
		case STATE_DOCUMENT:
			__handle_document(&ctx);
			break;
		case STATE_GLOBAL_MAP:
			__handle_global_map(&ctx);
			break;
		case STATE_KEY:
			__handle_global_key(&ctx);
			break;
		case STATE_FAMILY:
			__handle_family(&ctx);
			break;
		case STATE_SERIAL:
			__handle_serial(&ctx);
			break;
		case STATE_CRC8:
			/* XXX: ignore for now; we calculate it. */
			ctx.state     = STATE_KEY;
			ctx.key_seen |= SEEN_CRC8;
			break;
		case STATE_PRNG_COUNTER:
			__handle_prng(&ctx);
			break;
		case STATE_WRITE_CYCLE_MAP:
			__handle_write_cycle_map(&ctx);
			ctx.key_seen |= SEEN_WRITE_CYCLE;
			break;
		case STATE_WRITE_CYCLE_KEY:
			__handle_write_cycle_key(&ctx);
			break;
		case STATE_WRITE_CYCLE_VALUE:
			__handle_write_cycle_value(&ctx);
			break;
		case STATE_NVRAM_MAP:
			__handle_nvram_map(&ctx);
			ctx.key_seen |= SEEN_NVRAM;
			break;
		case STATE_NVRAM_KEY:
			__handle_nvram_key(&ctx);
			break;
		case STATE_NVRAM_VALUE:
			__handle_nvram_value(&ctx);
			break;
		case STATE_SECRET_MAP:
			__handle_secret_map(&ctx);
			ctx.key_seen |= SEEN_SECRET;
			break;
		case STATE_SECRET_KEY:
			__handle_secret_key(&ctx);
			break;
		case STATE_SECRET_VALUE:
			__handle_secret_value(&ctx);
			break;
		case STATE_DOCUMENT_END:
			__handle_document_end(&ctx);
			break;
		case STATE_STREAM_END:
			__handle_stream_end(&ctx);
			break;
		default:
			fprintf(stderr, "unknown state: %d\n", ctx.state);
			abort();
		}
	} while (ctx.event.type != YAML_STREAM_END_EVENT);

	fclose(fp);

	dev->family         = ctx.family;
	dev->prng_counter   = ctx.prng_counter;
	memcpy(dev->data_wc,       ctx.data_wc,   sizeof dev->data_wc);
	memcpy(dev->secret_wc,     ctx.secret_wc, sizeof dev->secret_wc);
	memcpy(dev->data_memory,   ctx.nvram,     sizeof dev->data_memory);
	memcpy(dev->secret_memory, ctx.secret,    sizeof dev->secret_memory);

	/* Serial is a special case, as it is stored LSB first.  For
	 * presentation it has been reversed, so do this here too.
	 */
	for (int i = 0; i < sizeof dev->serial; i++)
		dev->serial[i] = ctx.serial[sizeof(ctx.serial) - 1 - i];

	return 0;
}
