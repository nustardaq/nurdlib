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

#include <util/funcattr.h>
#include <time.h>

#ifndef KEEP_GMTIME_R
#	ifdef gmtime_r
#		undef gmtime_r
#	endif
#	define gmtime_r PLEASE_USE_gmtime_r_
#endif

struct	tm *gmtime_r_(time_t const *, struct tm *);
double	time_getd(void) FUNC_RETURNS;
char	*time_gets(void) FUNC_RETURNS;
int	time_sleep(double);

#endif
