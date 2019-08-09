#ifndef __COROUTINE_H
#define __COROUTINE_H

#include <ucontext.h>
#include "debug.h"
#include "list.h"

struct coroutine
{
	void			*cookie;
	void			*data;
	struct list_head	entry;
	struct list_head	yield_list;
	ucontext_t		context;
#ifdef DEBUG
	int			valgrind_stack_id;
#endif
};

typedef void (*coroutine_handler_t)(struct coroutine *);

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
