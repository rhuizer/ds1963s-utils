#ifndef __DS1963S_ERROR_H
#define __DS1963S_ERROR_H

#define DS1963S_ERROR_SUCCESS		0
#define DS1963S_ERROR_OPENCOM		1
#define DS1963S_ERROR_NO_DS2480		2
#define DS1963S_ERROR_INVALID_ADDRESS	3
#define DS1963S_ERROR_SET_CONTROL_BITS	4
#define DS1963S_ERROR_SET_LEVEL		5
#define DS1963S_ERROR_NOT_FOUND		6

#ifdef __cplusplus
extern "C" {
#endif

extern __thread int ds1963s_errno;

void ds1963s_perror(const char *s);

#ifdef __cplusplus
};
#endif

#endif
