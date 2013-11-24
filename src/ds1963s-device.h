#ifndef __DS1963S_DEVICE_H
#define __DS1963S_DEVICE_H

struct ds1963s_device
{
	uint8_t		serial[6];
	uint8_t		data_memory[512];
	uint8_t		secret_memory[64];
	uint8_t		scratchpad[32];
	uint32_t	data_wc[8];
	uint32_t	secret_wc[8];
	uint32_t	prng;

	uint8_t		TA1;
	uint8_t		TA2;

	uint8_t		M:1;
	uint8_t		X:1;
	uint8_t		HIDE:1;
	uint8_t		CHLG:1;
	uint8_t		AUTH:1;
	uint8_t		MATCH:1;
};

#endif
