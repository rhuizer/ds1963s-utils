#include <stdlib.h>
#include "ds2480-device.h"
#include "transport-factory.h"
#include "transport-unix.h"

int main(void)
{
	struct ds2480_device ds2480_dev;
	struct transport *t;

	if ( (t = transport_factory_new(TRANSPORT_UNIX)) == NULL) {
		perror("transport_factory_new()");
		exit(EXIT_FAILURE);
	}

	transport_unix_connect(t, "/tmp/centos7-serial");

	ds2480_dev_init(&ds2480_dev);
	ds2480_dev_run(&ds2480_dev, t);

	transport_destroy(t);
}
