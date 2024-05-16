/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2024
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

#ifndef UTIL_TIME_H
#define UTIL_TIME_H

#include <nconf/util/time.h>
#include <util/funcattr.h>

#if NCONF_mTIME_GET_bCLOCK_GETTIME
/* NCONF_CPPFLAGS=-D_POSIX_C_SOURCE=199309 */
#	define TIME_CLOCK_GETTIME 1
#elif NCONF_mTIME_GET_bCLOCK_GETTIME_LRT
/* NCONF_CPPFLAGS=-D_POSIX_C_SOURCE=199309 */
/* NCONF_LIBS=-lrt */
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
#	if NCONFING_mTIME_DRAFT9
#		include <time.h>
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
#	if NCONFING_mTIME_DRAFT9
#		include <time.h>
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	time_t tt;
	char buf[26];
	time(&tt);
	return NULL != asctime_r(localtime(&tt), buf, 0);
}
#	endif
#endif

double	time_getd(void) FUNC_RETURNS;
char	*time_gets(void) FUNC_RETURNS;
int	time_sleep(double);

#endif
