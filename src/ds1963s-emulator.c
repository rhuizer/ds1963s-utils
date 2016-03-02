#include <stdlib.h>
#include "ds2480-device.h"
#include "transport-factory.h"
#include "transport-unix.h"

int main(void)
{
	unsigned char buf[128];
	struct transport *t;

	if ( (t = transport_factory_new(TRANSPORT_UNIX)) == NULL) {
		perror("transport_factory_new()");
		exit(EXIT_FAILURE);
	}

	transport_unix_connect(t, "/tmp/centos7-serial");

	ds2480_dev_detect_handle(NULL, t);

	transport_destroy(t);
}
