#ifndef __DS1963S_ERROR_H
#define __DS1963S_ERROR_H

#define DS1963S_ERROR_SUCCESS		0
#define DS1963S_ERROR_OPENCOM		1
#define DS1963S_ERROR_NO_DS2480		2
#define DS1963S_ERROR_INVALID_ADDRESS	3
#define DS1963S_ERROR_SET_CONTROL_BITS	4
#define DS1963S_ERROR_SET_LEVEL		5
#define DS1963S_ERROR_NOT_FOUND		6
#define DS1963S_ERROR_SECRET_NUM	7	/* Invalid secret.          */
#define DS1963S_ERROR_SECRET_LEN	8	/* Invalid secret length.   */
#define DS1963S_ERROR_DATA_LEN		9	/* Invalid data length.     */
#define DS1963S_ERROR_ACCESS		10	/* Access to device failed. */
#define DS1963S_ERROR_TX_BLOCK		11	/* Failed to TX data block. */
#define DS1963S_ERROR_SP_ERASE		12	/* Failed to erase scratch. */
#define DS1963S_ERROR_SP_READ		13	/* Failed to read scratch.  */
#define DS1963S_ERROR_INTEGRITY		14	/* Data integrity error.    */
#define DS1963S_ERROR_SP_COPY		15	/* Failed to copy scratch.  */
#define DS1963S_ERROR_INVALID_PAGE	16	/* Invalid page number.     */

#ifdef __cplusplus
extern "C" {
#endif

void ds1963s_perror(int errno, const char *s);

#ifdef __cplusplus
};
#endif

#endif
