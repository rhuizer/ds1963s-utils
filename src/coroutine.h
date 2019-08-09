#ifndef __COROUTINE_H
#define __COROUTINE_H

#include <ucontext.h>
#include "list.h"

struct coroutine
{
#ifdef DEBUG
	char			*name;
#endif
	ucontext_t		context;
	void			*data;
	struct list_head	entry;
	struct list_head	yield_list;
};

typedef void (*coroutine_handler_t)(struct coroutine *, void *);

#ifdef __cplusplus
extern "C" {
#endif

int   coroutine_init(struct coroutine *, coroutine_handler_t, void *);
void *coroutine_await(struct coroutine *, struct coroutine *);
int   coroutine_return(struct coroutine *, void *);
int   coroutine_returnto(struct coroutine *, struct coroutine *, void *);
int   coroutine_yieldto(struct coroutine *, struct coroutine *);
int   coroutine_yield(struct coroutine *);
void  coroutine_destroy(struct coroutine *);

int   coroutine_main(void);

#ifdef __cplusplus
};
#endif

#endif
