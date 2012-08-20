#include <stdint.h>
#include "ibutton.h"
#include "ibutton/ownet.h"
#include "ibutton/shaib.h"

//#define SERIAL_PORT "/dev/ttyS0"
#define SERIAL_PORT "/dev/ttyUSB0"

int ibutton_get_aes_key(const uint8_t *subkey, uint8_t *aes_key)
{
	uint8_t firstDataPage[32], firstScratchPad[32];
	SHACopr copr;

	memcpy(firstDataPage, subkey, 32);

	if ( (copr.portnum = owAcquireEx(SERIAL_PORT)) == -1)
		return -1;

	FindNewSHA(copr.portnum, copr.devAN, TRUE);
	owSerialNum(copr.portnum, copr.devAN, FALSE);

	WriteDataPageSHA18(copr.portnum, 0, firstDataPage, 0);
	memset(firstScratchPad, '\0', 32);
	memcpy(firstScratchPad+8, subkey+32, 15);

	WriteScratchpadSHA18(copr.portnum, 0, firstScratchPad, 32, 1);
	SHAFunction18(copr.portnum, 0xC3, 0, 1);
	ReadScratchpadSHA18(copr.portnum, 0, 0, firstScratchPad, 1);

	memset(firstDataPage, '\0', 32);
	WriteDataPageSHA18(copr.portnum, 0, firstDataPage, 0);
	memcpy(aes_key, firstScratchPad+8, 24);
	owRelease(copr.portnum);	

	return 0;
}

int ibutton_get_serial(uint8_t *buf)
{
	SHACopr copr;

	if ( (copr.portnum = owAcquireEx(SERIAL_PORT)) == -1)
		return -1;

	FindNewSHA(copr.portnum, copr.devAN, TRUE);
	ReadAuthPageSHA18(copr.portnum, 9, buf, NULL, FALSE);
	owRelease(copr.portnum);

	return 0;
}

int ibutton_secret_zero(int portnum, int pagenum, int secretnum, int resume)
{
	char secret_scratchpad[32];
	int addr = pagenum << 5;
	char secret_data[32];

	memset(secret_scratchpad, 0, 32);
	memset(secret_data, 0, 32);

	if (WriteDataPageSHA18(portnum, pagenum, secret_data, resume) == FALSE)
		return -1;

	if (WriteScratchpadSHA18(portnum, addr, secret_scratchpad, 32, TRUE) == FALSE)
		return -1;

	if (SHAFunction18(portnum, (uchar)SHA_COMPUTE_FIRST_SECRET, addr, TRUE) == FALSE)
		return -1;

	if (CopySecretSHA18(portnum, secretnum) == FALSE)
		return -1;

	return 0;
}

int
ibutton_scratchpad_write(int portnum, uint16_t address, const char *data, size_t len)
{
	uint8_t block[64];

	/* Make sure the data we provide fits. */
	if (len > sizeof(block) - 3)
		return -1;

	OWASSERT(SelectSHA(portnum), OWERROR_ACCESS_FAILED, FALSE);

	/* write scratchpad command */
	block[0] = CMD_WRITE_SCRATCHPAD;
	/* TA1, which fully holds the offset. */
	block[1] = address & 0xFF;
	/* TA2, which is unused. */
	block[2] = address >> 8;
	/* payload */
	memcpy(block + 3, data, len);

	OWASSERT(owBlock(portnum, 0, block, len + 3), OWERROR_BLOCK_FAILED, FALSE);
	owTouchReset(portnum);

	return 0;
}
