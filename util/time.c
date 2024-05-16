/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2023-2024
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

#include <util/time.h>

#if defined(TIME_CLOCK_GETTIME)
#	include <stdlib.h>
#	include <time.h>
#	include <util/err.h>
#	include <util/thread.h>
#elif defined(NCONF_mTIME_GET_bMACH)
#	include <stdlib.h>
#	include <mach/mach_time.h>
#	include <util/err.h>
#endif

#if defined(NCONF_mTIME_DRAFT9_bNO)
#	define ASCTIME_R(tm, buf) asctime_r(tm, buf)
#	define GMTIME_R(tt, tm) gmtime_r(tt, tm)
#elif defined(NCONF_mTIME_DRAFT9_bYES)
#	define ASCTIME_R(tm, buf) asctime_r(tm, buf, 26)
#	define GMTIME_R(tt, tm) gmtime_r(tm, tt)
#endif

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
		err_(EXIT_FAILURE, "clock_gettime");
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
			err_(EXIT_FAILURE, "mach_timebase_info");
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
		errx_(EXIT_FAILURE, "time_sleep(%f<0)", a_s);
	}
#if defined(NCONF_mTIME_SLEEP_bNANOSLEEP)
	{
		struct timespec ts;

		ts.tv_sec = a_s;
		ts.tv_nsec = 1e9 * (a_s - (unsigned)a_s);
		if (0 != nanosleep(&ts, NULL)) {
			warn_("nanosleep");
			return 0;
		}
	}
#endif
	return 1;
}
