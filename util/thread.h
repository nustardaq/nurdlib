/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2021, 2023-2024
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

#ifndef UTIL_THREAD_H
#define UTIL_THREAD_H

#include <nconf/util/thread.h>
#include <util/funcattr.h>

#if NCONF_mTHREAD_bST_OLD
/* NCONF_LDFLAGS=-mthreads */
#	include <pthread.h>
#	define PTHREADS
#	if NCONFING_mTHREAD
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	pthread_attr_t a;
	return 0 == pthread_attr_create(&a);
}
#	endif
#elif NCONF_mTHREAD_bST_NEW
/* NCONF_LDFLAGS=-mthreads */
#	include <pthread.h>
#	define PTHREADS
#	if NCONFING_mTHREAD
#		define NCONF_TEST nconf_test_()
static void *runner_(void *a_dummy) { return a_dummy; }
static int nconf_test_(void) {
	pthread_t thread;
	return 0 == pthread_create(&thread, NULL, runner_, NULL);
}
#	endif
#elif NCONF_mTHREAD_bPTHREAD
/* NCONF_CPPFLAGS=-pthread */
/* NCONF_LIBS=-pthread */
#	include <pthread.h>
#	define PTHREADS
#	if NCONFING_mTHREAD
#		define NCONF_TEST nconf_test_()
static void *runner_(void *a_dummy) { return a_dummy; }
static int nconf_test_(void) {
	pthread_t thread;
	return 0 == pthread_create(&thread, NULL, runner_, NULL);
}
#	endif
#endif

#if defined(PTHREADS)
struct CondVar {
	pthread_cond_t	cond;
};
struct Mutex {
	pthread_mutex_t	mutex;
};
struct Thread {
	pthread_attr_t	attr;
	pthread_t	thread;
};
#endif

typedef void (*ThreadCreateCallback)(void *);

int	thread_condvar_broadcast(struct CondVar *) FUNC_NONNULL(());
int	thread_condvar_clean(struct CondVar *) FUNC_NONNULL(());
int	thread_condvar_init(struct CondVar *) FUNC_NONNULL(()) FUNC_RETURNS;
int	thread_condvar_signal(struct CondVar *) FUNC_NONNULL(());
int	thread_condvar_wait(struct CondVar *, struct Mutex *)
	FUNC_NONNULL(());
int	thread_mutex_clean(struct Mutex *) FUNC_NONNULL(());
int	thread_mutex_init(struct Mutex *) FUNC_NONNULL(()) FUNC_RETURNS;
int	thread_mutex_is_locked(struct Mutex *) FUNC_NONNULL(()) FUNC_RETURNS;
int	thread_mutex_lock(struct Mutex *) FUNC_NONNULL(());
int	thread_mutex_unlock(struct Mutex *) FUNC_NONNULL(());
int	thread_clean(struct Thread *) FUNC_NONNULL(());
int	thread_start(struct Thread *, void (*)(void *), void *)
	FUNC_NONNULL((1, 2)) FUNC_RETURNS;
void	thread_start_callback_set(ThreadCreateCallback, void *);

#endif
