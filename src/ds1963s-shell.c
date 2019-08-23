/* ds1963s-shell.c
 *
 * A interactive utility for low-level ds1963s experimentation.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * Copyright (C) 2013-2019  Ronald Huizer <rhuizer@hexpedition.com>
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
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "ds1963s-client.h"
#include "ds1963s-common.h"

#define DEFAULT_SERIAL_PORT	"/dev/ttyUSB0"

#define STATE_DS2480B		0
#define STATE_DS1963S		1
#define STATE_SCRATCHPAD	2
#define STATE_MEMORY		3

void handle_scratchpad_command(char *);

static struct ds1963s_client client;
static int state = STATE_DS1963S;

static int
__cmd_is_scratchpad(const char *cmd)
{
	if (cmd[0] != 's') return 0;
	if (!strcmp(cmd, "scratchpad")) return 1;
	if (!strcmp(cmd, "sp")) return 1;
	return 0;
}

static int
__cmd_is_memory(const char *cmd)
{
	if (cmd[0] != 'm') return 0;
	if (!strcmp(cmd, "memory")) return 1;
	if (!strcmp(cmd, "mem")) return 1;
	if (!strcmp(cmd, "m")) return 1;
	return 0;
}

static int
__cmd_is_read_auth_page(const char *cmd)
{
	if (cmd[0] != 'r') return 0;
	if (!strcmp(cmd, "read-auth-page")) return 1;
	if (!strcmp(cmd, "rap")) return 1;
	if (!strcmp(cmd, "ra")) return 1;
	return 0;
}

static int
parse_uint(const char *s, unsigned int *p)
{
	unsigned int i;
	char *endptr;

	i = strtoul(s, &endptr, 0);
	if (*s == 0 || *endptr != 0) {
		printf("Invalid number \"%s\".\n", s);
		return -1;
	}

	*p = i;
	return 0;
}

static int
address_get(const char *function, int *address)
{
	unsigned int _address;
	char *s;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("%s expects an address\n", function);
		return -1;
	}

	if (parse_uint(s, &_address) == -1)
		return -1;

	if (_address > 0xFFFF) {
		printf("Address should be in range [0, 0xFFFF]\n");
		return -1;
	}

	*address = _address;
	return 0;
}

const char *
prompt(void)
{
	switch (state) {
	case STATE_DS2480B:
		return "ds2480b> ";
	case STATE_DS1963S:
		return "ds1963s> ";
	case STATE_SCRATCHPAD:
		return "scratchpad> ";
	case STATE_MEMORY:
		return "memory> ";
	}

	return NULL;
}

void
handle_scratchpad_copy(void)
{
	unsigned int address, es;
	char *s;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("copy expects an address\n");
		return;
	}

	if (parse_uint(s, &address) == -1)
		return;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("copy expects E/S\n");
		return;
	}

	if (parse_uint(s, &es) == -1)
		return;

	if (ds1963s_client_sp_copy(&client, address, es) == -1)
		ds1963s_client_perror(&client, NULL);
}

void
handle_scratchpad_erase(void)
{
	unsigned int address;
	char *s;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		address = 0;
	} else if (parse_uint(s, &address) == -1) {
		return;
	}

	if (ds1963s_client_sp_erase(&client, address) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}
}

void handle_scratchpad_match(void)
{
	uint8_t hash[20] = { 0 };
	int     ret;
	char *  s;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("match expects a 20 bytes of hexadecimal data\n");
		return;
	}

	hex_decode(hash, s, 20);

	if ( (ret = ds1963s_client_sp_match(&client, hash)) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}

	printf("%s\n", ret ? "MATCH" : "MISMATCH");
}

void
handle_scratchpad_read(void)
{
	ds1963s_client_sp_read_reply_t reply;
	uint8_t  TA1, TA2;
	int      page;

	if (ds1963s_client_sp_read(&client, &reply) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}

	ds1963s_address_to_ta(reply.address, &TA1, &TA2);
	page = ds1963s_address_to_page(reply.address);

	printf("Address: 0x%-5.4x (Page   : %d)\n", reply.address, page);
	printf("TA1    : 0x%-5.2x (", TA1);
	printf("T07:T00:");
	for (int i = 7; i >= 0; i--)
		printf("%s %d", i != 7 ? "," : "", (TA1 >> i) & 1);
	printf(")\n");

	printf("TA2    : 0x%-5.2x (", TA2);
	printf("T15:T08:");
	for (int i = 7; i >= 0; i--)
		printf("%s %d", i != 7 ? "," : "", (TA2 >> i) & 1);
	printf(")\n");

	printf("E/S    : 0x%.2x\n", reply.es);
	printf("E      : 0x%-5.2x (", reply.es & 0x1F);
	printf("E04:E00:");
	for (int i = 4; i >= 0; i--)
		printf("%s %d", i != 4 ? "," : "", (reply.es >> i) & 1);
	printf(")\n");
	printf("PF     : %d\n", !!(reply.es & 0x20));
	printf("AA     : %d\n", !!(reply.es & 0x80));

	printf("CRC16  : 0x%-5.4x (%s)\n",
	       reply.crc16, reply.crc_ok ? "OK" : "WRONG");

	printf("Offset : %-7d (", TA1 & 0x1F);
	printf("T04:T00:");
	for (int i = 4; i >= 0; i--)
		printf("%s %d", i != 4 ? "," : "", (TA1 >> i) & 1);
	printf(")\n");
	printf("Length : %-7d\n", 32 - (TA1 & 0x1F));

	printf("Data   : ");
	for (int i = 0; i < reply.data_size; i++)
		printf("%.2x", reply.data[i]);
	printf("\n");
}

void
handle_scratchpad_write(void)
{
	unsigned int address;
	uint8_t      data[32];
	char *       s;
	ssize_t      n;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("write expects an address\n");
		return;
	}

	if (parse_uint(s, &address) == -1)
		return;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("write expects 32 bytes of hexadecimal data\n");
		return;
	}

	if ( (n = hex_decode(data, s, 32)) < 0) {
		printf("Invalid hex data \"%s\".\n", s);
		return;
	}

	if (ds1963s_client_sp_write(&client, address, data, n) == -1)
		ds1963s_client_perror(&client, NULL);
}

void
handle_memory_read(void)
{
	unsigned int address, size;
	uint8_t      buf[157];
	char *       s;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("read expects an address\n");
		return;
	}

	if (parse_uint(s, &address) == -1)
		return;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("read expects a size\n");
		return;
	}

	if (parse_uint(s, &size) == -1)
		return;

	if (ds1963s_client_memory_read(&client, address, buf, size) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}

	hexdump(buf, size, address);
}

void
handle_read_auth_page(void)
{
	ds1963s_client_read_auth_page_reply_t reply;
	int address;

	if (address_get("read-auth-page", &address) == -1)
		return;

	if (ds1963s_client_read_auth(&client, address, &reply) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}

	printf("W/C page  : 0x%.8x\n", reply.data_wc);
	printf("W/C secret: 0x%.8x\n", reply.secret_wc);
	printf("CRC16     : 0x%.4x  (%s)\n",
		reply.crc16, reply.crc_ok ? "OK" : "WRONG");
	printf("Data      : ");
	for (int i = 0; i < reply.data_size; i++)
		printf("%.2x", reply.data[i]);
	printf("\n");
}

void
handle_memory_command(char *line)
{
	char *cmd;

	if ( (cmd = strtok(line, " \t")) == NULL) {
		state = STATE_MEMORY;
		return;
	}

	if (!strcmp(cmd, "help")) {
		printf("Lorem bla bla\n");
	} else if (!strcmp(cmd, "ds1963s")) {
		state = STATE_DS1963S;
	} else if (__cmd_is_scratchpad(cmd)) {
		handle_scratchpad_command(NULL);
	} else if (__cmd_is_memory(cmd)) {
		state = STATE_MEMORY;
	} else if (!strcmp(cmd, "read") || !strcmp(cmd, "r")) {
		handle_memory_read();
	} else if (__cmd_is_read_auth_page(cmd)) {
		handle_read_auth_page();
	}
}

void
handle_scratchpad_command(char *line)
{
	char *cmd;

	if ( (cmd = strtok(line, " \t")) == NULL) {
		state = STATE_SCRATCHPAD;
		return;
	}

	if (!strcmp(cmd, "help")) {
		printf("Lorem bla bla\n");
	} else if (!strcmp(cmd, "ds1963s")) {
		state = STATE_DS1963S;
	} else if (__cmd_is_scratchpad(cmd)) {
		state = STATE_SCRATCHPAD;
	} else if (__cmd_is_memory(cmd)) {
		handle_memory_command(NULL);
	} else if (!strcmp(cmd, "copy") || !strcmp(cmd, "c")) {
		handle_scratchpad_copy();
	} else if (!strcmp(cmd, "erase") || !strcmp(cmd, "e")) {
		handle_scratchpad_erase();
	} else if (!strcmp(cmd, "match") || !strcmp(cmd, "m")) {
		handle_scratchpad_match();
	} else if (!strcmp(cmd, "read") || !strcmp(cmd, "r")) {
		handle_scratchpad_read();
	} else if (!strcmp(cmd, "write") || !strcmp(cmd, "w")) {
		handle_scratchpad_write();
	}
}

void
handle_ds1963s_command(char *line)
{
	char *cmd;

	if ( (cmd = strtok(line, " \t")) == NULL)
		return;

	if (!strcmp(cmd, "help")) {
		printf("Lorem bla bla\n");
	} else if (__cmd_is_scratchpad(cmd)) {
		handle_scratchpad_command(NULL);
	} else if (__cmd_is_memory(cmd)) {
		handle_memory_command(NULL);
	}
}

void
handle_command(char *line)
{
	switch (state) {
	case STATE_DS1963S:
		handle_ds1963s_command(line);
		break;
	case STATE_SCRATCHPAD:
		handle_scratchpad_command(line);
		break;
	case STATE_MEMORY:
		handle_memory_command(line);
		break;
	}
}

int main(void)
{
	char *line;

	if (ds1963s_client_init(&client, DEFAULT_SERIAL_PORT) == -1) {
		ds1963s_client_perror(&client, "ds1963s_client_init()");
		exit(EXIT_FAILURE);
	}

	while ( (line = readline(prompt())) != NULL) {
		if (*line != 0) {
			add_history(line);
			handle_command(line);
		}
		free(line);
	}

	exit(EXIT_SUCCESS);
}
