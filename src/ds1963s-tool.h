#ifndef DS1963S_TOOL_H
#define DS1963S_TOOL_H

#include <inttypes.h>
#include "ds1963s-client.h"
#include "ds1963s-device.h"

#ifdef HAVE_LIBYAML
#include <yaml.h>
#endif

struct ds1963s_brute_secret
{
	int		state;
	uint8_t		target_hmac[4][20];
	uint8_t		secret[8];
};

struct ds1963s_brute
{
	int				log_fd;
	struct ds1963s_device		dev;
	struct ds1963s_brute_secret	secrets[8];
};

struct ds1963s_tool
{
	/* Client for communication with a DS1963S ibutton. */
	struct ds1963s_client client;

	/* Side-channel module used for dumping secrets. */
	struct ds1963s_brute brute;

	int verbose;

#ifdef HAVE_LIBYAML
	yaml_emitter_t	emitter;
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

void
ds1963s_tool_secrets_get(struct ds1963s_tool *tool,
                         struct ds1963s_rom *rom,
                         uint32_t counters[16]);

#ifdef __cplusplus
};
#endif

#endif
