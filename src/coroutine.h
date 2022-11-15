/* coroutine.h
 *
 * A small and simple coroutine library written in C.
 *
 * Dedicated to Yuzuyu Arielle Huizer.
 *
 * Copyright (C) 2016-2019  Ronald Huizer <rhuizer@hexpedition.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef COROUTINE_H
#define COROUTINE_H

#include <ucontext.h>
#include "debug.h"
#include "list.h"

struct coroutine;

typedef void (*coroutine_handler_t)(struct coroutine *);
typedef void (*coroutine_destructor_t)(struct coroutine *);

struct coroutine
{
	coroutine_destructor_t	destructor;
	void			*cookie;
	void			*data;
	struct list_head	entry;
	struct list_head	yield_list;
	ucontext_t		context;
#ifdef DEBUG
	int			stack_id;
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

int   coroutine_init(struct coroutine *, coroutine_handler_t, void *);
void  coroutine_destructor_set(struct coroutine *, coroutine_destructor_t);
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
