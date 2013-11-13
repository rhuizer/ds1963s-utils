#ifndef __DS1963S_H
#define __DS1963S_H

#include "ibutton/shaib.h"

#define DS1963S_SERIAL_SIZE		32
#define DS1963S_ROM_COMMAND_READ	0x33

#define WRITE_CYCLE_DATA_8	0
#define WRITE_CYCLE_DATA_9	1
#define WRITE_CYCLE_DATA_10	2
#define WRITE_CYCLE_DATA_11	3
#define WRITE_CYCLE_DATA_12	4
#define WRITE_CYCLE_DATA_13	5
#define WRITE_CYCLE_DATA_14	6
#define WRITE_CYCLE_DATA_15	7
#define WRITE_CYCLE_SECRET_0	8
#define WRITE_CYCLE_SECRET_1	9
#define WRITE_CYCLE_SECRET_2	10
#define WRITE_CYCLE_SECRET_3	11
#define WRITE_CYCLE_SECRET_4	12
#define WRITE_CYCLE_SECRET_5	13
#define WRITE_CYCLE_SECRET_6	14
#define WRITE_CYCLE_SECRET_7	15

struct ds1963s_client
{
	SHACopr copr;
	int	resume;
};

struct ds1963s_rom
{
	uint8_t		family;
	uint8_t		serial[6];
	uint8_t		crc;
};

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

int ds1963s_client_init(struct ds1963s_client *ctx, const char *device);
void ds1963s_client_destroy(struct ds1963s_client *ctx);
int ds1963s_client_page_to_address(int page);
int ds1963s_client_address_to_page(int address);
int ds1963s_scratchpad_write(struct ds1963s_client *ctx, uint16_t address,
                             const char *data, size_t len);
int ds1963s_scratchpad_write_resume(struct ds1963s_client *ctx,
                                    uint16_t address, const char *data,
                                    size_t len, int resume);
int ds1963s_client_rom_get(struct ds1963s_client *ctx, struct ds1963s_rom *rom);
int ds1963s_client_serial_get(struct ds1963s_client *ctx, uint8_t buf[6]);
int ds1963s_client_taes_get(struct ds1963s_client *ctx, uint16_t *addr, uint8_t *es);
int ds1963s_client_taes_print(struct ds1963s_client *ctx);

int ds1963s_client_memory_read(struct ds1963s_client *ctx, uint16_t address,
                               uint8_t *data, size_t size);
int ds1963s_client_memory_write(struct ds1963s_client *ctx, uint16_t address,
                                const uint8_t *data, size_t size);
uint32_t ds1963s_client_prng_get(struct ds1963s_client *ctx);

void ibutton_perror(const char *s);
uint32_t ibutton_write_cycle_get(int portnum, int write_cycle_type);
int ds1963s_write_cycle_get_all(struct ds1963s_client*, uint32_t [16]);

uint8_t ds1963s_crc8(const uint8_t *buf, size_t count);

#ifdef __cplusplus
};
#endif	/* __cplusplus */

#endif	/* !__DS1963S_H */
