/* ds1963s-shell.c
 *
 * A interactive utility for ds1963s experimentation.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * Copyright (C) 2019  Ronald Huizer <rhuizer@hexpedition.com>
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

static struct ds1963s_client client;

static int
__cmd_is_help(const char *cmd)
{
	if (cmd[0] != 'h') return 0;
	if (!strcmp(cmd, "h")) return 1;
	if (!strcmp(cmd, "help")) return 1;
	return 0;
}

static int
__cmd_is_set(const char *cmd)
{
	if (cmd[0] != 's') return 0;
	if (!strcmp(cmd, "set")) return 1;
	return 0;
}

static int
__cmd_is_write_scratchpad(const char *cmd)
{
	if (cmd[0] != 'w') return 0;
	if (!strcmp(cmd, "wsp")) return 1;
	if (!strcmp(cmd, "write-scratchpad")) return 1;
	return 0;

}

static int
__cmd_is_read_scratchpad(const char *cmd)
{
	if (cmd[0] != 'r') return 0;
	if (!strcmp(cmd, "rsp")) return 1;
	if (!strcmp(cmd, "read-scratchpad")) return 1;
	return 0;

}

static int
__cmd_is_copy_scratchpad(const char *cmd)
{
	if (cmd[0] != 'c') return 0;
	if (!strcmp(cmd, "csp")) return 1;
	if (!strcmp(cmd, "copy-scratchpad")) return 1;
	return 0;
}

static int
__cmd_is_read_memory(const char *cmd)
{
	if (cmd[0] != 'r') return 0;
	if (!strcmp(cmd, "rm")) return 1;
	if (!strcmp(cmd, "read-memory")) return 1;
	return 0;
}

static int
__cmd_is_erase_scratchpad(const char *cmd)
{
	if (cmd[0] != 'e') return 0;
	if (!strcmp(cmd, "esp")) return 1;
	if (!strcmp(cmd, "erase-scratchpad")) return 1;
	return 0;
}

static int
__cmd_is_match_scratchpad(const char *cmd)
{
	if (cmd[0] != 'm') return 0;
	if (!strcmp(cmd, "msp")) return 1;
	if (!strcmp(cmd, "match-scratchpad")) return 1;
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
__cmd_is_compute_first_secret(const char *cmd)
{
	if (cmd[0] != 'c') return 0;
	if (!strcmp(cmd, "compute-first-secret")) return 1;
	if (!strcmp(cmd, "cfs")) return 1;
	return 0;
}

static int
__cmd_is_compute_next_secret(const char *cmd)
{
	if (cmd[0] != 'c') return 0;
	if (!strcmp(cmd, "compute-next-secret")) return 1;
	if (!strcmp(cmd, "cns")) return 1;
	return 0;
}

static int
__cmd_is_validate_data_page(const char *cmd)
{
	if (cmd[0] != 'v') return 0;
	if (!strcmp(cmd, "validate-data-page")) return 1;
	if (!strcmp(cmd, "validate")) return 1;
	if (!strcmp(cmd, "vdp")) return 1;
	return 0;
}

static int
__cmd_is_sign_data_page(const char *cmd)
{
	if (cmd[0] != 's') return 0;
	if (!strcmp(cmd, "sign-data-page")) return 1;
	if (!strcmp(cmd, "sign")) return 1;
	if (!strcmp(cmd, "sdp")) return 1;
	return 0;
}

static int
__cmd_is_compute_challenge(const char *cmd)
{
	if (cmd[0] != 'c') return 0;
	if (!strcmp(cmd, "compute-challenge")) return 1;
	if (!strcmp(cmd, "challenge")) return 1;
	if (!strcmp(cmd, "cc")) return 1;
	return 0;
}

static int
__cmd_is_authenticate_host(const char *cmd)
{
	if (cmd[0] != 'a') return 0;
	if (!strcmp(cmd, "authenticate-host")) return 1;
	if (!strcmp(cmd, "auth")) return 1;
	if (!strcmp(cmd, "ah")) return 1;
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
bool_get(const char *function, int *value)
{
	unsigned int _value;
	char *s;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("%s expects a boolean\n", function);
		return -1;
	}

	if (parse_uint(s, &_value) == -1)
		return -1;

	if (_value > 1) {
		printf("A boolean should be either 0 or 1.\n");
		return -1;
	}

	*value = _value;
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

void
handle_copy_scratchpad(void)
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
handle_erase_scratchpad(void)
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

void handle_match_scratchpad(void)
{
	uint8_t hash[20] = { 0 };
	int     ret;
	char *  s;

	if ( (s = strtok(NULL, " \t")) == NULL || strlen(s) != 40) {
		printf("match expects a 20 bytes of hexadecimal data\n");
		return;
	}

	if (hex_decode(hash, s, 20) == -1) {
		printf("match expects a 20 bytes of hexadecimal data\n");
		return;
	}

	if ( (ret = ds1963s_client_sp_match(&client, hash)) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}

	printf("%s\n", ret ? "MATCH" : "MISMATCH");
}

void
handle_read_scratchpad(void)
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
handle_write_scratchpad(void)
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
handle_read_memory(void)
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
handle_compute_first_secret(void)
{
	int address;

	if (address_get("compute-first-secret", &address) == -1)
		return;

	if (ds1963s_client_secret_compute_first(&client, address) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}
}

void
handle_compute_next_secret(void)
{
	int address;

	if (address_get("compute-next-secret", &address) == -1)
		return;

	if (ds1963s_client_secret_compute_next(&client, address) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}
}

void
handle_validate_data_page(void)
{
	int address;

	if (address_get("validate-data-page", &address) == -1)
		return;

	if (ds1963s_client_validate_data_page(&client, address) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}
}

void
handle_sign_data_page(void)
{
	int address;

	if (address_get("sign-data-page", &address) == -1)
		return;

	if (ds1963s_client_sign_data_page(&client, address) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}
}

static void
__warn_page_0_8(int address)
{
	int is_p0, is_p8;

	is_p0 = address >=   0 && address <  32;
	is_p8 = address >= 256 && address < 288;

	if (is_p0 || is_p8)
		printf("INFO: The datasheet specifies this function will fail "
		       "when used on pages 0 and 8.\n");
}

void
handle_compute_challenge(void)
{
	int address;

	if (address_get("compute-challenge", &address) == -1)
		return;

	if (ds1963s_client_compute_challenge(&client, address) == -1) {
		ds1963s_client_perror(&client, NULL);
		__warn_page_0_8(address);
		return;
	}
}

void
handle_authenticate_host(void)
{
	int address;

	if (address_get("authenticate-host", &address) == -1)
		return;

	if (ds1963s_client_authenticate_host(&client, address) == -1) {
		ds1963s_client_perror(&client, NULL);
		__warn_page_0_8(address);
		return;
	}
}

void
handle_main_help(void)
{
	printf("List of commands:\n\n");
	printf("set                       -- Manage setting variables\n");
	printf("help, h                   -- This help menu\n");

	printf("List of memory commands:\n\n");
	printf("write-scratchpad,     wsp -- Write data to the scratchpad\n");
	printf("read-scratchpad,      rsp -- Read data from the scratchpad\n");
	printf("copy-scratchpad,      csp -- Copy scratchpad data to memory\n");
	printf("read-memory,          rm  -- Read data from memory\n");
	printf("erase-scratchpad,     esp -- Erase the scratchpad\n");
	printf("match-scratchpad,     msp -- Match the scratchpad to data\n");
	printf("read-auth-page,       rap -- Read authenticated page\n");

	printf("\nList of SHA commands:\n\n");
	printf("compute-first-secret, cfs -- Compute the first secret\n");
	printf("compute-next-secret,  cns -- Compute the next secret\n");
	printf("validate-data-page,   vdp -- Validate a page in memory\n");
	printf("sign-data-page,       sdp -- Sign a page in memory\n");
	printf("compute-challenge,    cc  -- Compute a challenge\n");
	printf("authenticate-host,    ah  -- Authenticate a host\n");
}

void
handle_help(void)
{
	char *arg;

	if ( (arg = strtok(NULL, " \t")) == NULL) {
		handle_main_help();
		return;
	}

	if (__cmd_is_write_scratchpad(arg)) {
		printf("write-scratchpad, wsp <addr> <data>\n\n");
		printf("This command writes the hex encoded data in `data' "
		       "to the scratchpad at address\n");
		printf("`addr'.  Only T4:T0 (that is, the least significant "
		       "5 bits of the address) are\n");
		printf("used and encode the scratchpad offset to start "
		       "writing to.  Data is encoded in\n");
		printf("plain hexadecimal format.\n\n");
		printf("Note that the scratchpad area is 32-bytes large.\n\n");
		printf("Examples:\n\n");
		printf("- wsp 0 41424344\n");
		printf("  Writes the string \"ABCD\" to the scratchpad "
		       "starting at offset 0.\n\n");
		printf("- wsp 24 0000000000000000\n");
		printf("  Sets the last 8 bytes of the scratchpad to 0.\n");
	} else if (__cmd_is_read_scratchpad(arg)) {
		printf("read-scratchpad, rsp\n\n");
		printf("This command will read the TA1, TA2, and E/S "
		       "registers and scratchpad data\n");
		printf("starting at offset T4:T0 from the device and "
		       "display the results.\n\n");
		printf("Note that because no address is provided, the "
		       "scratchpad data offset is\n");
		printf("based on a previous value of the T4:T0.\n\n");
		printf("The output is verbose, and presents collected "
		       "information in various forms.\n");
	} else if (__cmd_is_copy_scratchpad(arg)) {
		printf("copy-scratchpad, csp <addr> <es>\n\n");
		printf("This command copies data from the scratchpad to "
		       "memory.  The address in `addr'\n");
		printf("and the E/S byte in `es' are used as an "
		       "authorization pattern.  If they are\n");
		printf("equal to the address in TA1 and TA2, and the E/S "
		       "byte on the device then the\n");
		printf("copy will proceed.  In this case the AA flag will "
		       "be set.\n\n");
		printf("The source of the copy starts at offset TA4:TA0 and "
		       "ends at offset E/S,\n");
		printf("inclusive.  The destination of the copy starts at "
		       "the address (TA2:TA1) and\n");
		printf("ends at offset E/S in the page of that address.\n\n");
		printf("Example:\n\n");
		printf("esp\n");
		printf("wsp 63 41\n");
		printf("csp 63 31\n");
		printf("rsp\n\n");
		printf("This sequence will erase the scratchpad, set the "
		       "address to 63, write the\n");
		printf("character 'A' to offset 31 (T4:T0 -- the 5 least "
		       "significant bits of 63) and\n");
		printf("set E/S to 31.  The copy command will then authorize "
		       "using the same values as\n");
		printf("in TA2:TA1 and E/S, and copy the byte at scratchpad "
		       "offset 31 to memory address\n");
		printf("63.  The copy ends because E/S is 31.  The AA flag "
		       "should be set in the final\n");
		printf("rsp command.\n");
	} else if (__cmd_is_read_memory(arg)) {
		printf("read-memory, rm <addr> <size>\n\n");
		printf("This command will read `size' bytes of data "
		       "starting at address `addr' and\n");
		printf("display a hexdump of the data read.\n\n");
		printf("The address registers on the device will be updated "
		       "to point to the last\n");
		printf("address read from.  The E/S byte is not affected.\n\n");
		printf("Examples:\n\n");
		printf("- rm 0 32\n");
		printf("  Display a hexdump of 32-bytes of data at address "
		       "0.\n\n");
		printf("- rm 0x0200 0\n");
		printf("This will not read actual data but just set TA2 to"
		       "0x02 and TA1 to 0x00.\n");
	} else if (__cmd_is_erase_scratchpad(arg)) {
		printf("erase-scratchpad, esp [addr]\n\n");
		printf("This command will erase the full 32-byte scratchpad "
		       "by filling it with 0xFF\n");
		printf("bytes.  The address in `addr' is latched into the "
		       "TA2 and TA1 registers but is not used\n");
		printf("any further.  If `addr' is not specified it will use "
		       "the default value of 0.\n");
		printf("Finally the HIDE flag will be set to 0, which is the "
		       "only way this can be done.\n\n");
		printf("Examples:\n\n");
		printf("- esp\n");
		printf("  Will set all scratchpad bytes to 0xFF, TA2 to 0x00, "
		       "and TA1 to 0x00.\n\n");
		printf("- esp 0x123\n");
		printf("  Will set all scratchpad bytes to 0xFF, TA2 to 0x01, "
		       "and TA2 to 0x23.\n");
	} else if (__cmd_is_match_scratchpad(arg)) {
		printf("match-scratchpad, esp <data>\n\n");
		printf("This command will compare 20 bytes of hex encoded "
		       "data in `data' to 20 bytes on\n");
		printf("the scratchpad starting at offset 8.  This scratchpad "
		       "location is used by other\n");
		printf("functions to store a 20 byte SHA-1 hash.\n\n");
		printf("Examples:\n\n");
		printf("- msp 4141414141414141414142424242424242424242\n");
		printf("  Will compare bytes at scratchpad[8] through "
		       "scratchpad[27] (inclusive) to the\n");
		printf("  string \"AAAAAAAAAABBBBBBBBBB\" and report whether "
		       "there was a match or a mismatch.\n");
	} else if (__cmd_is_read_auth_page(arg)) {
		printf("read-auth-page, rap <addr>\n\n");
		printf("This command will read data at address `addr' up to "
		       "and including offset 31 in\n");
		printf("the page that address is in and return it.  "
		       "Furthermore, the write-cycle\n");
		printf("counter of the page and the write-cycle counter of "
		       "the secret associated with\n");
		printf("that page will be returned.\n\n");
		printf("Finally, a 20 byte SHA-1 based message authentication "
		       "code over the full page\n");
		printf("data (not just the data read/returned), the secret "
		       "associated with the page,\n");
		printf("the write cycle-counters, the page number, the device "
		       "serial, and 3 bytes of\n");
		printf("scratchpad data is stored on the scratchpad starting "
		       "at offset 8.\n\n");
		printf("Examples:\n\n");
		printf("- rap 0\n");
		printf("  Reads 32 bytes of data on page 0, the W/C of page "
		       "0, and the W/C of secret 0.\n");
		printf("  Stores the MAC as discussed above starting at "
		       "SP[8].\n\n");
		printf("- rap 31\n");
		printf("  Reads 1 byte of data from address 31, the W/C of "
		       "page 0, and the W/C of\n");
		printf("  secret 0.  Stores the MAC as discussed above "
		       "starting at SP[8].\n");
	}
}

void
handle_set(void)
{
	char *arg;
	int   b;

	if ( (arg = strtok(NULL, " \t")) == NULL) {
		printf("resume: %d\n", client.resume);
		return;
	}

	if (!strcmp(arg, "resume")) {
		if (bool_get("set resume", &b) == -1)
			return;
		client.resume = b;
	} else {
		printf("Unknown setting: \"%s\".  Try \"help\".\n", arg);
	}
}

void
handle_command(char *line)
{
	char *cmd;

	if ( (cmd = strtok(line, " \t")) == NULL)
		return;

	if (__cmd_is_help(cmd)) {
		handle_help();
	} else if (__cmd_is_set(cmd)) {
		handle_set();
	} else if (__cmd_is_write_scratchpad(cmd)) {
		handle_write_scratchpad();
	} else if (__cmd_is_read_scratchpad(cmd)) {
		handle_read_scratchpad();
	} else if (__cmd_is_copy_scratchpad(cmd)) {
		handle_copy_scratchpad();
	} else if (__cmd_is_read_memory(cmd)) {
		handle_read_memory();
	} else if (__cmd_is_erase_scratchpad(cmd)) {
		handle_erase_scratchpad();
	} else if (__cmd_is_match_scratchpad(cmd)) {
		handle_match_scratchpad();
	} else if (__cmd_is_read_auth_page(cmd)) {
		handle_read_auth_page();
	} else if (__cmd_is_compute_first_secret(cmd)) {
		handle_compute_first_secret();
	} else if (__cmd_is_compute_next_secret(cmd)) {
		handle_compute_next_secret();
	} else if (__cmd_is_validate_data_page(cmd)) {
		handle_validate_data_page();
	} else if (__cmd_is_sign_data_page(cmd)) {
		handle_sign_data_page();
	} else if (__cmd_is_compute_challenge(cmd)) {
		handle_compute_challenge();
	} else if (__cmd_is_authenticate_host(cmd)) {
		handle_authenticate_host();
	} else {
		printf("Unknown command: \"%s\".  Try \"help\".\n", cmd);
	}
}

int main(void)
{
	char *line;

	if (ds1963s_client_init(&client, DEFAULT_SERIAL_PORT) == -1) {
		ds1963s_client_perror(&client, "ds1963s_client_init()");
		exit(EXIT_FAILURE);
	}

	while ( (line = readline("ds1963s> ")) != NULL) {
		if (*line != 0) {
			add_history(line);
			handle_command(line);
		}
		free(line);
	}

	exit(EXIT_SUCCESS);
}
