/* ds1963s-device.c
 *
 * A software implementation of the DS1963S iButton.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * -- Ronald Huizer / r.huizer@xs4all.nl / 2013
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ds1963s-device.h"
#include "getput.h"
#include "sha1.h"

/* Calculate the value for MPX, as used in __sha1_get_input_1. */
static uint8_t __mpx_get(int M, int X, uint8_t *SP)
{
	return (M << 7) | (X << 6) | (SP[12] & 0x1F);
}

/* Calculate the value for MP, as used in __sha1_get_input_2. */
static uint8_t __mp_get(int M, int X, int page)
{
	return (M << 7) | (X << 6) | (page & 0xF);
}

/* Construct the SHA-1 input for the following operations:
 *
 * - Validate Data Page
 * - Sign Data Page
 * - Authenticate Host
 * - Compute First Secret
 * - Compute Next Secret
 */
static void __sha1_get_input_1(uint8_t M[64], uint8_t *SS, uint8_t *PP,
                               uint8_t MPX, uint8_t *SP)
{
	memcpy(&M[ 0], &SS[ 0],  4);
	memcpy(&M[ 4], &PP[ 0], 32);
	memcpy(&M[36], &SP[ 8],  4);
	M[40] = MPX;
	memcpy(&M[41], &SP[13],  7);
	memcpy(&M[48], &SS[ 4],  4);
	memcpy(&M[52], &SP[20],  3);
	M[55] = 0x80;
	M[56] = 0;
	M[57] = 0;
	M[58] = 0;
	M[59] = 0;
	M[60] = 0;
	M[61] = 0;
	M[62] = 1;
	M[63] = 0xB8;
}

/* Construct the SHA-1 input for the following operations:
 *
 * - Read Authenticated Page
 * - Compute Challenge
 */
static void __sha1_get_input_2(uint8_t M[64], uint8_t *SS, uint8_t *CC,
                               uint8_t *PP, uint8_t FAMC, uint8_t MP,
                               uint8_t *SN, uint8_t *SP)
{
	printf("CC[0]: %x\n", CC[0]);

	memcpy(&M[ 0], &SS[ 0],  4);
	memcpy(&M[ 4], &PP[ 0], 32);
	memcpy(&M[36], &CC[ 0],  4);
	M[40] = MP;
	M[41] = FAMC;
	memcpy(&M[42], &SN[ 0],  6);
	memcpy(&M[48], &SS[ 4],  4);
	memcpy(&M[52], &SP[20],  3);
	M[55] = 0x80;
	M[56] = 0;
	M[57] = 0;
	M[58] = 0;
	M[59] = 0;
	M[60] = 0;
	M[61] = 0;
	M[62] = 1;
	M[63] = 0xB8;
}

/* Construct the SHA-1 output for all operation except:
 *
 * - Compute First Secret
 * - Compute Next Secret
 */
static void __sha1_get_output_1(uint8_t SP[32], uint32_t A, uint32_t B,
                                uint32_t C, uint32_t D, uint32_t E)
{
	SP[ 8] = (E >>  0) & 0xFF;
	SP[ 9] = (E >>  8) & 0xFF;
	SP[10] = (E >> 16) & 0xFF;
	SP[11] = (E >> 24) & 0xFF;

	SP[12] = (D >>  0) & 0xFF;
	SP[13] = (D >>  8) & 0xFF;
	SP[14] = (D >> 16) & 0xFF;
	SP[15] = (D >> 24) & 0xFF;

	SP[16] = (C >>  0) & 0xFF;
	SP[17] = (C >>  8) & 0xFF;
	SP[18] = (C >> 16) & 0xFF;
	SP[19] = (C >> 24) & 0xFF;

	SP[20] = (B >>  0) & 0xFF;
	SP[21] = (B >>  8) & 0xFF;
	SP[22] = (B >> 16) & 0xFF;
	SP[23] = (B >> 24) & 0xFF;

	SP[24] = (A >>  0) & 0xFF;
	SP[25] = (A >>  8) & 0xFF;
	SP[26] = (A >> 16) & 0xFF;
	SP[27] = (A >> 24) & 0xFF;
}

void ds1963s_dev_read_auth_page(struct ds1963s_device *ds1963s, int page)
{
	uint8_t M[64];
	uint8_t CC[4];
	SHA1_CTX ctx;

	/* XXX: set properly. */
	ds1963s->M = 0;

	ds1963s->X = 0;
	ds1963s->CHLG = 0;
	ds1963s->AUTH = 0;

	PUT_32BIT_LSB(CC, ds1963s->data_wc[page]);

	__sha1_get_input_2(
		M,
		ds1963s->secret_memory,
		CC,
		ds1963s->data_memory,
		DS1963S_DEVICE_FAMILY,
		__mp_get(ds1963s->M, ds1963s->X, page),
		ds1963s->serial,
		ds1963s->scratchpad
	);

	/* We omit the finalize, as the DS1963S does not use it, but rather
	 * uses the internal state A, B, C, D, E for the result.  This also
	 * means we have to subtract the initial state from the context, as
	 * it has added these to the results.
	 */
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, M, sizeof M);

	/* Write the results to the scratchpad. */
	__sha1_get_output_1(
		ds1963s->scratchpad, 
		ctx.state[0] - 0x67452301,
		ctx.state[1] - 0xEFCDAB89,
		ctx.state[2] - 0x98BADCFE,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);
}

void ds1963s_sign_page(struct ds1963s_device *ds1963s)
{
	uint8_t M[64];
	SHA1_CTX ctx;

	/* XXX: set properly. */
	ds1963s->M = 0;

	ds1963s->X = 0;
	ds1963s->CHLG = 0;
	ds1963s->AUTH = 0;

	__sha1_get_input_1(
		M,
		ds1963s->secret_memory,
		ds1963s->data_memory,
		__mpx_get(ds1963s->M, ds1963s->X, ds1963s->scratchpad),
		ds1963s->scratchpad
	);

	/* We omit the finalize, as the DS1963S does not use it, but rather
	 * uses the internal state A, B, C, D, E for the result.  This also
	 * means we have to subtract the initial state from the context, as
	 * it has added these to the results.
	 */
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, M, sizeof M);

	/* Write the results to the scratchpad. */
	__sha1_get_output_1(
		ds1963s->scratchpad, 
		ctx.state[0] - 0x67452301,
		ctx.state[1] - 0xEFCDAB89,
		ctx.state[2] - 0x98BADCFE,
		ctx.state[3] - 0x10325476,
		ctx.state[4] - 0xC3D2E1F0
	);
}

#ifdef DS1963S_DEVICE_TEST
int main(void)
{
	struct ds1963s_device ds1963s;

	memset(&ds1963s, 0, sizeof ds1963s);
	memset(ds1963s.scratchpad, 0xff, sizeof ds1963s.scratchpad);
	memcpy(ds1963s.secret_memory, "\x00\x00\x01\x00\x01\x05\x07\x00", 8);

	ds1963s_sign_page(&ds1963s);
	int i;

	for (i = 0; i < 32; i++)
		printf("%.2x", ds1963s.scratchpad[i]);
	printf("\n");

	/* Test initialization for read auth page. */
	memset(&ds1963s, 0, sizeof ds1963s);
	memset(ds1963s.scratchpad, 0xff, sizeof ds1963s.scratchpad);
	ds1963s.data_wc[0] = 1;
	memcpy(ds1963s.serial, "\x8e\x4f\x68\x00\x00\x00", 6);
	memcpy(ds1963s.secret_memory, "\x00\x00\x01\x00\x01\x05\x07\x00", 8);
	ds1963s_read_auth_page(&ds1963s, 0);

	for (i = 0; i < 32; i++)
		printf("%.2x", ds1963s.scratchpad[i]);
	printf("\n");
}
#endif
