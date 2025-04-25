/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2023-2025
 * Håkan T Johansson
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

#include <nconf/util/time.c>

#if NCONF_mTIME_GET_bCLOCK_GETTIME
#	define _POSIX_C_SOURCE 199309L
#	define TIME_CLOCK_GETTIME 1
#elif NCONF_mTIME_GET_bCLOCK_GETTIME_LRT
/* NCONF_LIBS=-lrt */
#	define _POSIX_C_SOURCE 199309L
#	define TIME_CLOCK_GETTIME 1
#elif NCONF_mTIME_GET_bMACH
#	if NCONFING_mTIME_GET
#		define NCONF_TEST mach_absolute_time()
#	endif
#endif

#if NCONFING_mTIME_GET && TIME_CLOCK_GETTIME
#	include <time.h>
#	define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	struct timespec ts;
	return 0 == clock_gettime(CLOCK_REALTIME, &ts);
}
#endif

#if NCONF_mTIME_SLEEP_bNANOSLEEP
#endif
#if NCONFING_mTIME_SLEEP
#	include <time.h>
#	define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	struct timespec ts = {0, 0};
	return 0 == nanosleep(&ts, NULL);
}
#endif

#if NCONF_mTIME_DRAFT9_bNO
/* NCONF_NOEXEC */
#	include <time.h>
#	define ASCTIME_R(tm, buf) asctime_r(tm, buf)
#	define GMTIME_R(tt, tm) gmtime_r(tt, tm)
char *asctime_r(const struct tm *tm, char *buf);
struct tm *gmtime_r(const time_t *clock, struct tm *result);
#	if NCONFING_mTIME_DRAFT9
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	time_t tt;
	char buf[26];
	time(&tt);
	return NULL != asctime_r(localtime(&tt), buf);
}
#	endif
#elif NCONF_mTIME_DRAFT9_bYES
/* NCONF_NOEXEC */
#	include <time.h>
#	define ASCTIME_R(tm, buf) asctime_r(tm, buf, 26)
#	define GMTIME_R(tt, tm) gmtime_r(tm, tt)
#	if NCONFING_mTIME_DRAFT9
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	time_t tt;
	char buf[26];
	time(&tt);
	return NULL != asctime_r(localtime(&tt), buf, 0);
}
#	endif
#elif NCONF_mTIME_DRAFT9_bFREEBSD
/* NCONF_NOEXEC */
#	undef _POSIX_C_SOURCE
#	define _POSIX_C_SOURCE 199506
#	include <time.h>
#	define ASCTIME_R(tm, buf) asctime_r(tm, buf)
#	define GMTIME_R(tt, tm) gmtime_r(tt, tm)
#	if NCONFING_mTIME_DRAFT9
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	time_t tt;
	char buf[26];
	time(&tt);
	return NULL != asctime_r(localtime(&tt), buf, 0);
}
#	endif
#endif

#if !NCONFING

#include <time.h>
#if TIME_CLOCK_GETTIME
#	include <stdlib.h>
#	include <util/thread.h>
#elif NCONF_mTIME_GET_bMACH
#	include <stdlib.h>
#	include <mach/mach_time.h>
#endif

#include <nurdlib/log.h>
#define KEEP_GMTIME_R
#include <util/time.h>

struct tm *
gmtime_r_(time_t const *a_tt, struct tm *a_tm)
{
	return GMTIME_R(a_tt, a_tm);
}

double
time_getd(void)
{
#if defined(TIME_CLOCK_GETTIME)
	enum MutexState {
		MUTEX_UNINITED,
		MUTEX_OK,
		MUTEX_FAIL
	};
	static enum MutexState s_mutex_state = MUTEX_UNINITED;
	static struct Mutex s_mutex;
	clockid_t s_clockid =
#	if defined(CLOCK_MONOTONIC)
	    CLOCK_MONOTONIC;
#	else
	    CLOCK_REALTIME;
#	endif
	struct timespec tp;

	if (MUTEX_UNINITED == s_mutex_state) {
		s_mutex_state = thread_mutex_init(&s_mutex) ? MUTEX_OK :
		    MUTEX_FAIL;
		/*
		 * If the mutex init failed, then we shouldn't see more than
		 * one thread entering here, so let's keep going without using
		 * the mutex.
		 */
	}
	if (MUTEX_OK == s_mutex_state) {
		thread_mutex_lock(&s_mutex);
	}
	for (;;) {
		if (0 == clock_gettime(s_clockid, &tp)) {
			break;
		}
#	if defined(CLOCK_MONOTONIC)
		if (CLOCK_MONOTONIC == s_clockid) {
			s_clockid = CLOCK_REALTIME;
			continue;
		}
#	endif
		log_err(LOGL, "clock_gettime");
	}
	if (MUTEX_OK == s_mutex_state) {
		thread_mutex_unlock(&s_mutex);
	}
	return tp.tv_sec + 1e-9 * tp.tv_nsec;
#elif defined(NCONF_mTIME_GET_bMACH)
	uint64_t mach_time;
	static double scaling_factor = -1.0;

	mach_time = mach_absolute_time();
	if (0.0 > scaling_factor) {
		mach_timebase_info_data_t timebase_info;
		kern_return_t ret;

		ret = mach_timebase_info(&timebase_info);
		if (0 != ret) {
			log_err(LOGL, "mach_timebase_info");
		}
		scaling_factor = 1e-9 * timebase_info.numer /
		    timebase_info.denom;
	}

	return mach_time * scaling_factor;
#else
#	error "This is cannot be!"
#endif
}

char *
time_gets(void)
{
	struct tm tm;
	time_t tt;
	char *buf;

	time(&tt);
	GMTIME_R(&tt, &tm);
	buf = malloc(26);
	ASCTIME_R(&tm, buf);
	return buf;
}

int
time_sleep(double a_s)
{
	if (0.0 > a_s) {
		log_error(LOGL, "time_sleep(%f<0)", a_s);
	}
#if defined(NCONF_mTIME_SLEEP_bNANOSLEEP)
	{
		struct timespec ts;

		ts.tv_sec = a_s;
		ts.tv_nsec = 1e9 * (a_s - (unsigned)a_s);
		if (0 != nanosleep(&ts, NULL)) {
			log_warn(LOGL, "nanosleep");
			return 0;
		}
	}
#else
#	error "This is cannot be!"
#endif
	return 1;
}

#endif
