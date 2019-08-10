/* coroutine.c
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
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "coroutine.h"
#include "debug.h"
#include "list.h"

#ifdef DEBUG
#include <valgrind/valgrind.h>
#endif

static size_t stack_size = 65536;
static ucontext_t main_context;
static ucontext_t cleanup_context;
static struct coroutine *coro_current;
static struct list_head active_list = LIST_HEAD_INIT(active_list);

int coroutine_init(struct coroutine *coro, coroutine_handler_t f, void *cookie)
{
	unsigned char *stack;

	if (getcontext(&coro->context) == -1)
		return -1;

	if ( (stack = malloc(stack_size)) == NULL)
		return -1;

	coro->cookie                    = cookie;
	coro->data			= NULL;
	coro->context.uc_link           = &cleanup_context;
	coro->context.uc_stack.ss_sp    = stack;
	coro->context.uc_stack.ss_size  = stack_size;
	coro->context.uc_stack.ss_flags = 0;
	list_add_tail(&coro->entry, &active_list);
	list_init(&coro->yield_list);

#ifdef DEBUG
	coro->stack_id = VALGRIND_STACK_REGISTER(stack, stack + stack_size);
#endif

	makecontext(&coro->context, (void (*)())f, 1, coro);

	return 0;
}

void coroutine_reschedule(struct coroutine *coro)
{
	coro_current = coro;
	list_del(&coro->entry);
	list_add_tail(&coro->entry, &active_list);
}

void *coroutine_await(struct coroutine *coro, struct coroutine *other)
{
	/* coroutine_await should not be called on an inactive coroutine. */
	/* XXX: test this. */

	/* Remove the coroutine waiting from the active_list. */
	list_del(&coro->entry);

	/* Insert the coroutine waiting into the yield list of the
	 * coroutine it waits on.
	 */
	list_add_tail(&coro->entry, &other->yield_list);

	coroutine_reschedule(other);
	if (swapcontext(&coro->context, &other->context) == -1)
		return NULL;

	/* Take it from the yield list and reschedule. */
	coroutine_reschedule(coro);

	return other->data;
}

/* Yield to an arbitrary other coroutine. */
int coroutine_yield(struct coroutine *coro)
{
	struct coroutine *other;

	/* Nothing to reschedule to, so we're done. */
	if (list_empty(&active_list))
		return 0;

	other = list_entry(active_list.next, struct coroutine, entry);
	return coroutine_yieldto(coro, other);
}

/* Yield to a specific other coroutine. */
int coroutine_yieldto(struct coroutine *coro, struct coroutine *other)
{
	assert(coro != NULL);
	assert(other != NULL);

	coroutine_reschedule(other);
	if (swapcontext(&coro->context, &other->context) == -1)
		return -1;

	return 0;
}

/* Return data to a coroutine waiting for this one. */
int coroutine_return(struct coroutine *coro, void *data)
{
	struct coroutine *other;

	/* If there is no coroutine waiting on this yield, schedule the next
	 * routine from the active_list.
	 */
	if (list_empty(&coro->yield_list))
		return coroutine_yield(coro);

	other = list_entry(coro->yield_list.next, struct coroutine, entry);

	/* Remove the selected coroutine from the yield list. */
	list_del(&other->entry);
	list_add_tail(&other->entry, &active_list);

	return coroutine_returnto(coro, other, data);
}

int coroutine_returnto(struct coroutine *coro, struct coroutine *other, void *data)
{
	/* XXX: Would like to test if 'other' on yield_list of 'coro' ... */
	coro->data = data;

	coroutine_reschedule(other);
	if (swapcontext(&coro->context, &other->context) == -1)
		return -1;

	return 0;
}

void coroutine_destroy(struct coroutine *coro)
{
#ifdef DEBUG
	VALGRIND_STACK_DEREGISTER(coro->stack_id);
#endif
	list_del(&coro->entry);
	free(coro->context.uc_stack.ss_sp);
}

void coroutine_end(void)
{
	coroutine_destroy(coro_current);
}

int coroutine_main(void)
{
	static unsigned char stack[65536];
	struct coroutine *coro;
	int id;

	/* This is the place where we store and will return to the main
	 * context once the active_list is empty.  We cannoy modify the
	 * context after we set it in the uc_link of another context which
	 * we then makecontext(), so this has to be done here.
	 */
	getcontext(&main_context);
	if (list_empty(&active_list))
		return 0;

	getcontext(&cleanup_context);
	cleanup_context.uc_link           = &main_context;
	cleanup_context.uc_stack.ss_sp    = stack;
	cleanup_context.uc_stack.ss_size  = sizeof(stack);
	cleanup_context.uc_stack.ss_flags = 0;
#ifdef DEBUG
	id = VALGRIND_STACK_REGISTER(stack, stack + sizeof stack);
#endif
	makecontext(&cleanup_context, coroutine_end, 0);

	/* Main loop; this is only relevant when coroutine_end is called,
	 * as that is the only way we will reenter the cleanup_context above.
	 */
	if (!list_empty(&active_list)) {
		coro = list_entry(active_list.next, struct coroutine, entry);
		coroutine_reschedule(coro);
		setcontext(&coro->context);
	}

#ifdef DEBUG
	VALGRIND_STACK_DEREGISTER(id);
#endif
	return 0;
}

#ifdef TEST
struct coroutine coro_f;
struct coroutine coro_g;
struct coroutine coro_h;

void f(struct coroutine *coro)
{
	int i;

	printf("cookie: %p\n", coro->cookie);

	for (i = 0; i < 50; i++) {
		printf("coroutine f\n");
		coroutine_return(coro, (void *)i);
		printf("after yield\n");
	}
}

void g(struct coroutine *coro)
{
	void *ret;
	int i;

	printf("coroutine g\n");
	printf("g: main: %p\n", &main_context);
	printf("g: main.uc_link: %p\n", main_context.uc_link);
	printf("g: cleanup: %p\n", &cleanup_context);
	printf("g: link: %p\n", coro->context.uc_link);

	coroutine_return(coro, NULL);
}

void h(struct coroutine *coro)
{
	void *ret;
	int i;

	for (i = 0; i < 50; i++) {
		printf("coroutine h\n");
		ret = coroutine_await(coro, &coro_f);
		printf("coroutine h got: %d\n", ret);
	}
}

int main(void)
{
	coroutine_init(&coro_f, f, 0x41414141);
	coroutine_init(&coro_g, g, NULL);
	coroutine_init(&coro_h, h, NULL);

	coroutine_main();
	printf("back in main()\n");
}
#endif
