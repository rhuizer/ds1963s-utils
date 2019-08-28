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
#include <getopt.h>
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
__cmd_is_rom(const char *cmd)
{
	if (cmd[0] != 'r') return 0;
	if (!strcmp(cmd, "rom")) return 1;
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
__cmd_is_sha(const char *cmd)
{
	if (cmd[0] != 's') return 0;
	if (!strcmp(cmd, "sha-command")) return 1;
	if (!strcmp(cmd, "sha")) return 1;
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
byte_get(const char *function, uint8_t *value)
{
	unsigned int _value;
	char *s;

	if ( (s = strtok(NULL, " \t")) == NULL) {
		printf("%s expects a byte\n", function);
		return -1;
	}

	if (parse_uint(s, &_value) == -1)
		return -1;

	if (_value > 255) {
		printf("A byte should be less than or equal to 255.\n");
		return -1;
	}

	*value = _value;
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
address_symbolic_get(const char *s)
{
	if (!strcmp(s, "pctr") || !strcmp(s, "prng") ||
	    !strcmp(s, "PRNG")) {
		return 0x2a0;
	} else if (!strcmp(s, "hash")) {
		return 0x248;
	} else if (!strcmp(s, "sp") || !strcmp(s, "scratchpad")) {
		return 0x240;
	}

	return -1;
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

	if ( (_address = address_symbolic_get(s)) != -1) {
		*address = _address;
		return 0;
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
		printf("write expects up to 32 bytes of hexadecimal data\n");
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
	uint8_t      buf[157];
	int          address;
	unsigned int size;
	char *       s;

	if (address_get("read-memory", &address) == -1)
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

void handle_sha_command(void)
{
	int     address;
	uint8_t cmd;

	if (byte_get("sha-command", &cmd) == -1)
		return;

	if (address_get("sha-command", &address) == -1)
		return;

	if (ds1963s_client_sha_command(&client, cmd, address) == -1) {
		ds1963s_client_perror(&client, NULL);
		return;
	}
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
	printf("rom                       -- Lists the ROM serial\n");
	printf("set                       -- Manage setting variables\n");
	printf("help, h                   -- This help menu\n");

	printf("\nList of memory commands:\n\n");
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
		printf("write-scratchpad, wsp <addr> <data>\n\n"

		       "This command writes the hex encoded data in `data' "
		       "to the scratchpad at address\n"
		       "`addr'.  Only T4:T0 (that is, the least significant "
		       "5 bits of the address) are\n"
		       "used and encode the scratchpad offset to start "
		       "writing to.  Data is encoded in\n"
		       "plain hexadecimal format.\n\n"

		       "Note that the scratchpad area is 32-bytes large.\n\n"

		       "Examples:\n\n"

		       "- wsp 0 41424344\n"
		       "  Writes the string \"ABCD\" to the scratchpad "
		       "starting at offset 0.\n\n"
		       "- wsp 24 0000000000000000\n"
		       "  Sets the last 8 bytes of the scratchpad to 0.\n"
		);
	} else if (__cmd_is_read_scratchpad(arg)) {
		printf("read-scratchpad, rsp\n\n"

		       "This command will read the TA1, TA2, and E/S "
		       "registers and scratchpad data\n"
		       "starting at offset T4:T0 from the device and "
		       "display the results.\n\n"

		       "Note that because no address is provided, the "
		       "scratchpad data offset is\n"
		       "based on a previous value of the T4:T0.\n\n"

		       "The output is verbose, and presents collected "
		       "information in various forms.\n"
		);
	} else if (__cmd_is_copy_scratchpad(arg)) {
		printf("copy-scratchpad, csp <addr> <es>\n\n"

		       "This command copies data from the scratchpad to "
		       "memory.  The address in `addr'\n"
		       "and the E/S byte in `es' are used as an "
		       "authorization pattern.  If they are\n"
		       "equal to the address in TA1 and TA2, and the E/S "
		       "byte on the device then the\n"
		       "copy will proceed.  In this case the AA flag will "
		       "be set.\n\n"

		       "The source of the copy starts at offset TA4:TA0 and "
		       "ends at offset E/S,\n"
		       "inclusive.  The destination of the copy starts at "
		       "the address (TA2:TA1) and\n"
		       "ends at offset E/S in the page of that address.\n\n"

		       "Example:\n\n"

		       "esp\n"
		       "wsp 63 41\n"
		       "csp 63 31\n"
		       "rsp\n\n"
		       "This sequence will erase the scratchpad, set the "
		       "address to 63, write the\n"
		       "character 'A' to offset 31 (T4:T0 -- the 5 least "
		       "significant bits of 63) and\n"
		       "set E/S to 31.  The copy command will then authorize "
		       "using the same values as\n"
		       "in TA2:TA1 and E/S, and copy the byte at scratchpad "
		       "offset 31 to memory address\n"
		       "63.  The copy ends because E/S is 31.  The AA flag "
		       "should be set in the final\n"
		       "rsp command.\n"
		);
	} else if (__cmd_is_read_memory(arg)) {
		printf("read-memory, rm <addr> <size>\n\n"
		       "This command will read `size' bytes of data "
		       "starting at address `addr' and\n"
		       "display a hexdump of the data read.\n\n"

		       "The address registers on the device will be updated "
		       "to point to the last\n"
		       "address read from.  The E/S byte is not affected.\n\n"

		       "Examples:\n\n"

		       "- rm 0 32\n"
		       "  Display a hexdump of 32-bytes of data at address "
		       "0.\n\n"

		       "- rm 0x0200 0\n"
		       "This will not read actual data but just set TA2 to"
		       "0x02 and TA1 to 0x00.\n"
		);
	} else if (__cmd_is_erase_scratchpad(arg)) {
		printf("erase-scratchpad, esp [addr]\n\n"

		       "This command will erase the full 32-byte scratchpad "
		       "by filling it with 0xFF\n"
		       "bytes.  The address in `addr' is latched into the "
		       "TA2 and TA1 registers but is not used\n"
		       "any further.  If `addr' is not specified it will use "
		       "the default value of 0.\n"
		       "Finally the HIDE flag will be set to 0, which is the "
		       "only way this can be done.\n\n"

		       "Examples:\n\n"

		       "- esp\n"
		       "  Will set all scratchpad bytes to 0xFF, TA2 to 0x00, "
		       "and TA1 to 0x00.\n\n"

		       "- esp 0x123\n"
		       "  Will set all scratchpad bytes to 0xFF, TA2 to 0x01, "
		       "and TA1 to 0x23.\n"
		);
	} else if (__cmd_is_match_scratchpad(arg)) {
		printf("match-scratchpad, esp <data>\n\n"

		       "This command will compare 20 bytes of hex encoded "
		       "data in `data' to 20 bytes on\n"
		       "the scratchpad starting at offset 8.  This scratchpad "
		       "location is used by other\n"
		       "functions to store a 20 byte SHA-1 hash.\n"
		       "If the AUTH flag was set, the MATCH flag will be "
		       "set.\n\n"

		       "Examples:\n\n"

		       "- msp 4141414141414141414142424242424242424242\n"
		       "  Will compare bytes at scratchpad[8] through "
		       "scratchpad[27] (inclusive) to the\n"
		       "  string \"AAAAAAAAAABBBBBBBBBB\" and report whether "
		       "there was a match or a mismatch.\n"
		);
	} else if (__cmd_is_read_auth_page(arg)) {
		printf("read-auth-page, rap <addr>\n\n"

		       "This command will read data at address `addr' up to "
		       "and including offset 31 in\n"
		       "the page that address is in and return it.  "
		       "Furthermore, the write-cycle\n"
		       "counter of the page and the write-cycle counter of "
		       "the secret associated with\n"
		       "that page will be returned.\n\n"

		       "Finally, a 20 byte SHA-1 based message authentication "
		       "code over the full page\n"
		       "data (not just the data read/returned), the secret "
		       "associated with the page,\n"
		       "the write cycle-counters, the page number, the device "
		       "serial, and 3 bytes of\n"
		       "scratchpad data is stored on the scratchpad starting "
		       "at offset 8.\n\n"

		       "Examples:\n\n"

		       "- rap 0\n"
		       "  Reads 32 bytes of data on page 0, the W/C of page "
		       "0, and the W/C of secret 0.\n"
		       "  Stores the MAC as discussed above starting at "
		       "SP[8].\n\n"

		       "- rap 31\n"
		       "  Reads 1 byte of data from address 31, the W/C of "
		       "page 0, and the W/C of\n"
		       "  secret 0.  Stores the MAC as discussed above "
		       "starting at SP[8].\n"
		);
	} else if (__cmd_is_compute_first_secret(arg)) {
		printf("compute-first-secret, cfs <addr>\n\n"

		       "This command computes a SHA-1 based message "
		       "authentication code over a NULL\n"
		       "secret (that is, eight 0x00 bytes), the full "
		       "32-bytes data of the page that the\n"
		       "address `addr' is in, and 15 bytes of scratchpad "
		       "data from SP[8] to SP[22]\n"
		       "(inclusive).  The address is latched into registers "
		       "TA2 and TA1 on the device.\n\n"

		       "The result of this function repeatedly stores 8 bytes "
		       "of the resulting MAC on\n"
		       "the scratchpad until it is filled.  This will happen "
		       "4 times in total.\n\n"

		       "The HIDE flag will be set, so afterwards data from "
		       "the scratchpad can be copied\n"
		       "to secret memory using the copy-scratchpad command "
		       "immediately.  The repetition\n"
		       "above means we data can be written to any of the "
		       "secrets, and up to 4 secrets\n"
		       "can be written at once.\n\n"

		       "Examples:\n\n"
		       "- cfs 0\n"
		       "  Calculates the SHA-1 MAC over a NULL secret, the "
		       "data in page 0, and 15 bytes\n"
		       "  of scratchpad data.  The scratchpad is filled by "
		       "storing 8 bytes of the\n"
		       "  calculated MAC 4 times.  TA2 and TA1 will both me "
		       "set to 0.\n\n"

		       "- cfs 0x13\n"
		       "  Calculates the SHA-1 MAC over a NULL secret, the "
		       "data in page 0, and 15 bytes\n"
		       "  of scratchpad data.  The scratchpad is filled by "
		       "storing 8 bytes of the\n"
		       "  calculated MAC 4 times.  TA2 will be set to 0, TA1 "
		       "will be set to 0x13.\n"
		);
	} else if (__cmd_is_compute_next_secret(arg)) {
		printf("compute-next-secret, cns <addr>\n\n"

		       "This command computes a SHA-1 based message "
		       "authentication code over the secret\n"
		       "associated with `addr', the full 32-bytes data of the "
		       "page that the address\n"
		       "`addr' is in, and 15 bytes of scratchpad data from "
		       "SP[8] to SP[22] (inclusive).\n"
		       "The address is latched into registers TA2 and TA1 on "
		       "the device.\n\n"

		       "The result of this function repeatedly stores 8 bytes "
		       "of the resulting MAC on\n"
		       "the scratchpad until it is filled.  This will happen "
		       "4 times in total.\n\n"

		       "The HIDE flag will be set, so afterwards data from "
		       "the scratchpad can be copied\n"
		       "to secret memory using the copy-scratchpad command "
		       "immediately.  The repetition\n"
		       "above means we data can be written to any of the "
		       "secrets, and up to 4 secrets\n"
		       "can be written at once.\n\n"

		       "Examples:\n\n"

		       "- cns 0\n"
		       "  Calculates the SHA-1 MAC over secret 0, the "
		       "data in page 0, and 15 bytes\n"
		       "  of scratchpad data.  The scratchpad is filled by "
		       "storing 8 bytes of the\n"
		       "  calculated MAC 4 times.  TA2 and TA1 will both me "
		       "set to 0.\n\n"

		       "- cns 0x13\n"
		       "  Calculates the SHA-1 MAC over a secret 0, the "
		       "data in page 0, and 15 bytes\n"
		       "  of scratchpad data.  The scratchpad is filled by "
		       "storing 8 bytes of the\n"
		       "  calculated MAC 4 times.  TA2 will be set to 0, TA1 "
		       "will be set to 0x13.\n"
		);
	} else if (__cmd_is_validate_data_page(arg)) {
		printf("validate-data-page, validate, vdp <addr>\n\n"

		       "This command will calculate a SHA-1 message "
		       "authentication code over the full\n"
		       "32-byte data in the page that `addr' belongs to, "
		       "the associated secret, and 15\n"
		       "bytes of scratchpad data from SP[8] to SP[22] "
		       "(inclusive).  This 160-bit SHA-1\n"
		       "MAC is then stored at SP[8] to SP[20] (inclusive) and "
		       "the HIDE flag is set.\n"
		       "TA1 and TA2 will be set to the address `addr'.\n\n"

		       "The command can be used to validate data in a data "
		       "page.  It is meant to be\n"
		       "used in combination with the match-scratchpad "
		       "function, and can be used to\n"
		       "prove to the device that a secret associated with a "
		       "data page is known.\n\n"

		       "Example:\n\n"
		       "  ds1963s> esp\n"
		       "  ds1963s> sdp 0\n"
		       "  ds1963s> rsp\n"
		       "  Address: 0x0000  (Page   : 0)\n"
		       "  TA1    : 0x00    (T07:T00: 0, 0, 0, 0, 0, 0, 0, 0)\n"
		       "  TA2    : 0x00    (T15:T08: 0, 0, 0, 0, 0, 0, 0, 0)\n"
		       "  E/S    : 0x1f\n"
		       "  E      : 0x1f    (E04:E00: 1, 1, 1, 1, 1)\n"
		       "  PF     : 0\n"
		       "  AA     : 0\n"
		       "  CRC16  : 0x33e1  (OK)\n"
		       "  Offset : 0       (T04:T00: 0, 0, 0, 0, 0)\n"
		       "  Length : 32\n"
		       "  Data   : ffffffffffffffff42bd425d27fb70e43df23ab8aed"
		       "59b90d81b718cffffffff\n"
		       "  ds1963s> esp\n"
		       "  ds1963s> vdp 0\n"
		       "  ds1963s> msp 42bd425d27fb70e43df23ab8aed59b90d81b71"
		       "8c\n"
		       "  MATCH\n"
		);
	} else if (__cmd_is_sign_data_page(arg)) {
		printf("sign-data-page, sign, sdp <addr>\n\n"

		       "This command will calculate a SHA-1 message "
		       "authentication code over the full\n"
                       "32-byte data in the page that `addr' belongs to, the "
		       "associated secret, and 15\n"
		       "bytes of scratchpad data from SP[8] to SP[22] "
		       "(inclusive).  This 160-bit SHA-1\n"
		       "MAC is then stored at SP[8] to SP[20] (inclusive).  "
		       "TA1 and TA2 will be set to\n"
		       "the address `addr'.\n\n"

		       "The command can be used to sign data in a data page, "
		       "and this signature can\n"
		       "then be verified by anyone who knows the secret.\n\n"

			"Examples:\n\n"

		        "- sdp 0\n"
		        "  Will calculate a SHA-1 MAC over the 32-bytes in "
			"page 0, secret 0, and the\n"
		        "  data at SP[8] to SP[20] (inclusive).  This MAC is "
			"stored at SP[8] to SP[20]\n"
		        "  inclusive.  TA2 is set to 0 and TA1 is set to 0."
			"\n\n"

			"- sdp 0x13\n"
		        "  Will calculate a SHA-1 MAC over the 32-bytes in "
			"page 0, secret 0, and the\n"
		        "  data at SP[8] to SP[20] (inclusive).  This MAC is "
			"stored at SP[8] to SP[20]\n"
		        "  inclusive.  TA2 is set to 0 and TA1 is set to 0x13."
			"\n"
		);
	} else if (__cmd_is_compute_challenge(arg)) {
		printf("This is a stub entry.\n");
	} else if (__cmd_is_authenticate_host(arg)) {
		printf("This is a stub entry.\n");
	} else {
		printf("Unknown command: \"%s\".  Try \"help\".\n", arg);
	}
}

void
handle_rom(void)
{
        ds1963s_rom_t rom;

        ds1963s_client_rom_get(&client, &rom);

	printf("Data  : ");
	for (int i = 0; i < sizeof rom.raw; i++)
		printf("%.2x", rom.raw[i]);
	printf("\n");

	printf("Family: 0x%.2x\n", rom.family);

	printf("Serial: ");
	for (int i = 0; i < sizeof rom.serial; i++)
		printf("%.2x", rom.serial[i]);
	printf("\n");

	printf("CRC8  : 0x%.2x  (%s)\n", rom.crc, rom.crc_ok ? "OK" : "WRONG");
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
	} else if (__cmd_is_rom(cmd)) {
		handle_rom();
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
	} else if (__cmd_is_sha(cmd)) {
		handle_sha_command();
	} else {
		printf("Unknown command: \"%s\".  Try \"help\".\n", cmd);
	}
}

void
usage(const char *progname)
{
	fprintf(stderr, "Use as: %s [OPTION]\n",
	        progname ? progname : "ds1963s-shell");
	fprintf(stderr, "Available options:\n");
	fprintf(stderr, "   -d --device=pathname  the serial device used.\n");
	fprintf(stderr, "   -h --help             this help menu.\n");
}

static const struct option options[] = {
	{ "device", 1, NULL, 'd' },
	{ "help",   0, NULL, 'h' }
};

const char optstr[] = "d:h";

const char banner[] =
"`7MM\"\"\"Yb.    .M\"\"\"bgd                  .6*\"            .M\"\"\"bgd\n"
"  MM    `Yb. ,MI    \"Y __,            ,M'              ,MI    \"Y\n"
"  MM     `Mb `MMb.    `7MM  .d*\"*bg. ,Mbmmm.   pd\"\"b.  `MMb.\n"
"  MM      MM   `YMMNq.  MM 6MP    Mb 6M'  `Mb.(O)  `8b   `YMMNq.\n"
"  MM     ,MP .     `MM  MM YMb    MM MI     M8     ,89 .     `MM\n"
"  MM    ,dP' Mb     dM  MM  `MbmmdM9 WM.   ,M9   \"\"Yb. Mb     dM\n"
".JMMmmmdP'   P\"Ybmmd\" .JMML.     .M'  WMbmmd9       88 P\"Ybmmd\"\n"
"                               .d9            (O)  .M'\n"
"                             m\"'               bmmmd'\n"
"\n"
"     .M\"\"\"bgd `7MMF'  `7MMF'`7MM\"\"\"YMM  `7MMF'      `7MMF'\n"
"    ,MI    \"Y   MM      MM    MM    `7    MM          MM\n"
"    `MMb.       MM      MM    MM   d      MM          MM\n"
"      `YMMNq.   MMmmmmmmMM    MMmmMM      MM          MM\n"
"    .     `MM   MM      MM    MM   Y  ,   MM      ,   MM      ,\n"
"    Mb     dM   MM      MM    MM     ,M   MM     ,M   MM     ,M\n"
"    P\"Ybmmd\"  .JMML.  .JMML..JMMmmmmMMM .JMMmmmmMMM .JMMmmmmMMM\n"
"\n"
"                           For Yuzu\n"
"                       By Ronald Huizer\n\n";

int
main(int argc, char **argv)
{
	const char *device_name;
	char *line;
	int   i, o;

	device_name = DEFAULT_SERIAL_PORT;
	while ( (o = getopt_long(argc, argv, optstr, options, &i)) != -1) {
		switch (o) {
		case 'd':
			device_name = optarg;
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	printf("\n%s", banner);

	if (ds1963s_client_init(&client, device_name) == -1) {
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
