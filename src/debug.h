#ifndef DEBUG_H
#define DEBUG_H

#include <assert.h>

#ifndef NDEBUG
#define DEBUG
#define DEBUG_LOG(fmt, ...)						\
	do {								\
		fprintf(stderr, fmt, ## __VA_ARGS__);			\
	} while (0)
#else
#define DEBUG_LOG(fmt, ...)
#endif

#endif
