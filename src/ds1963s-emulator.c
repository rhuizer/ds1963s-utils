/* ds1963s-emulator.c
 *
 * A software emulator for a DS2048B and DS1963S 1-wire topology that can
 * connect to a serial device over a unix domain socket.
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
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "ds1963s-device.h"
#include "ds2480b-device.h"
#include "transport-factory.h"
#include "transport-unix.h"
#ifdef HAVE_LIBYAML
#include "ds1963s-emulator-yaml.h"
#endif

#define PROGNAME                "ds1963s-emulator"
#define UNIX_SOCKET_PATH	"/tmp/.ds2480-serial"

static const struct option options[] = {
	{ "config",             1,      NULL,   'c' },
	{ "device",             1,      NULL,   'd' },
	{ "help",               0,      NULL,   'h' }
};

const char optstr[] = "c:d:h";

void usage(const char *progname)
{
	fprintf(stderr, "Use as: %s [OPTION]\n", progname ?: PROGNAME);
	fprintf(stderr, "   -c --config=pathname  the configuration file to "
	                "use.\n");
	fprintf(stderr, "   -d --device=pathname  the unix socket to use as "
	                "serial device.\n");
	fprintf(stderr, "   -h --help             display the help menu.\n");
}

int main(int argc, char **argv)
{
	struct ds1963s_device ds1963s;
	struct ds2480b_device ds2480b;
	struct transport *serial;
	struct one_wire_bus bus;
	const char *config_name;
	const char *device_name;
	int i, o;

	config_name = NULL;
	device_name = UNIX_SOCKET_PATH;
	while ( (o = getopt_long(argc, argv, optstr, options, &i)) != -1) {
		switch (o) {
		case 'c':
			config_name = optarg;
			break;
		case 'd':
			device_name = optarg;
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	one_wire_bus_init(&bus);
	ds1963s_dev_init(&ds1963s);
	ds2480b_dev_init(&ds2480b);

	if (config_name != NULL) {
#ifdef HAVE_LIBYAML
		ds1963s_emulator_yaml_load(&ds1963s, config_name);
#else
		fprintf(stderr, "%s has been built without libyaml support.\n"
		                "libyaml is necessary for loading "
		                "configurations.\n", argv[0] ?: PROGNAME);
		exit(EXIT_FAILURE);
#endif
	}

	if ( (serial = transport_factory_new(TRANSPORT_UNIX)) == NULL) {
		perror("transport_factory_new()");
		exit(EXIT_FAILURE);
	}

	if (transport_unix_connect(serial, device_name) != 0) {
		perror("transport_unix_connect()");
		exit(EXIT_FAILURE);
	}

	/* Connect the ds2480b to the host serial port and the 1-wire bus. */
	ds2480b_dev_connect_serial(&ds2480b, serial);

	if (ds2480b_dev_bus_connect(&ds2480b, &bus) == -1) {
		fprintf(stderr, "Could not connect DS2480 to 1-wire bus.\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the ds1963s to the 1-wire bus. */
	ds1963s_dev_connect_bus(&ds1963s, &bus);

	/* Run the whole emulated bus topology. */
	one_wire_bus_run(&bus);

	transport_destroy(serial);
}
