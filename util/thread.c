/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2021, 2024
 * Hans Toshihide Törnqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include <util/thread.h>
#include <string.h>
#include <nurdlib/base.h>
#include <util/err.h>

#if NCONF_mTHREAD_bST_OLD
#	define DO_PTHREADS 1
#	define PTHREAD_STACK_MIN THREAD_DEFAULT_STACK
#	define ATTR_CREATE(ret, a) ret = pthread_attr_create(&a)
#	define ATTR_DESTROY(a)
#	define THREAD_CREATE(ret, t)\
    ret = pthread_create(&t->thread, t->attr, run, starter)
#elif NCONF_mTHREAD_bST_NEW
#	define DO_PTHREADS 1
#	define PTHREAD_STACK_MIN THREAD_DEFAULT_STACK
#	define ATTR_CREATE(ret, a) ret = pthread_attr_init(&a)
#	define ATTR_DESTROY(a) pthread_attr_destroy(&a)
#	define THREAD_CREATE(ret, t)\
    ret = pthread_create(&t->thread, &t->attr, run, starter)
#elif NCONF_mTHREAD_bPTHREAD
#	define DO_PTHREADS 1
#	include <limits.h>
#	define ATTR_CREATE(ret, a) ret = pthread_attr_init(&a)
#	define ATTR_DESTROY(a) pthread_attr_destroy(&a)
#	define THREAD_CREATE(ret, t)\
    ret = pthread_create(&t->thread, &t->attr, run, starter)
#endif

#if DO_PTHREADS

#	include <errno.h>

struct Starter {
	void	(*func)(void *);
	void	*data;
};

static void	*run(void *);

static ThreadCreateCallback g_thread_start_callback;
static void *g_thread_start_data;

void *
run(void *a_data)
{
	struct Starter starter;

	if (NULL != g_thread_start_callback) {
		g_thread_start_callback(g_thread_start_data);
	}

	memmove(&starter, a_data, sizeof starter);
	FREE(a_data);

	starter.func(starter.data);
	return NULL;
}

int
thread_condvar_broadcast(struct CondVar *a_condvar)
{
	if (0 != pthread_cond_broadcast(&a_condvar->cond)) {
		warn_("pthread_cond_broadcast");
		return 0;
	}
	return 1;
}

int
thread_condvar_clean(struct CondVar *a_condvar)
{
	if (0 != pthread_cond_destroy(&a_condvar->cond)) {
		warn_("pthread_cond_destroy");
		return 0;
	}
	return 1;
}

int
thread_condvar_init(struct CondVar *a_condvar)
{
	if (0 != pthread_cond_init(&a_condvar->cond, NULL)) {
		warn_("pthread_cond_init");
		return 0;
	}
	return 1;
}

int
thread_condvar_signal(struct CondVar *a_condvar)
{
	if (0 != pthread_cond_signal(&a_condvar->cond)) {
		warn_("pthread_cond_signal");
		return 0;
	}
	return 1;
}

int
thread_condvar_wait(struct CondVar *a_condvar, struct Mutex *a_mutex)
{
	if (0 != pthread_cond_wait(&a_condvar->cond, &a_mutex->mutex)) {
		warn_("pthread_cond_wait");
		return 0;
	}
	return 1;
}

int
thread_mutex_clean(struct Mutex *a_mutex)
{
	if (0 != pthread_mutex_destroy(&a_mutex->mutex)) {
		warn_("pthread_mutex_destroy");
		return 0;
	}
	return 1;
}

int
thread_mutex_init(struct Mutex *a_mutex)
{
	if (0 != pthread_mutex_init(&a_mutex->mutex, NULL)) {
		warn_("pthread_mutex_init");
		return 0;
	}
	return 1;
}

int
thread_mutex_is_locked(struct Mutex *a_mutex)
{
	int ret;

	ret = pthread_mutex_trylock(&a_mutex->mutex);
	if (EBUSY == ret) {
		return 1;
	}
	if (0 == ret) {
		thread_mutex_unlock(a_mutex);
	} else {
		warn_("pthread_mutex_trylock");
	}
	return 0;
}

int 
thread_mutex_lock(struct Mutex *a_mutex)
{
	if (0 != pthread_mutex_lock(&a_mutex->mutex)) {
		warn_("pthread_mutex_lock");
		return 1;
	}
	return 0;
}

int
thread_mutex_unlock(struct Mutex *a_mutex)
{
	if (0 != pthread_mutex_unlock(&a_mutex->mutex)) {
		warn_("pthread_mutex_unlock");
		return 1;
	}
	return 0;
}

int
thread_clean(struct Thread *a_thread)
{
	int ret;

	ret = 1;
	if (0 != pthread_join(a_thread->thread, NULL)) {
		warn_("pthread_destroy");
		ret = 0;
	}
	ATTR_DESTROY(a_thread->attr);
	return ret;
}

int
thread_start(struct Thread *a_thread, void (*a_func)(void *), void *a_data)
{
	struct Starter *starter;
	int ret;

	CALLOC(starter, 1);
	starter->func = a_func;
	starter->data = a_data;
	ATTR_CREATE(ret, a_thread->attr);
	if (0 != ret) {
		goto thread_start_fail;
	}
	THREAD_CREATE(ret, a_thread);
	if (0 != ret) {
		goto thread_start_fail;
	}
	return 1;
thread_start_fail:
	warn_("pthread_create");
	FREE(starter);
	return 0;
}

void
thread_start_callback_set(ThreadCreateCallback a_callback, void *a_data)
{
	g_thread_start_callback = a_callback;
	g_thread_start_data = a_data;
}

#endif
