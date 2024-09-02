/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021-2022, 2024
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

#ifndef UTIL_STRING_H
#define UTIL_STRING_H

#include <stdarg.h>
#include <string.h>
#include <util/funcattr.h>
#include <util/stdint.h>

#define snprintf PLEASE_USE_snprintf_
#define vsnprintf PLEASE_USE_vsnprintf_
int	snprintf_(char *, size_t, char const *, ...) FUNC_PRINTF(3, 4);
int	vsnprintf_(char *, size_t, char const *, va_list) FUNC_PRINTF(3, 0);

#define strdup PLEASE_USE_strdup_
#define strndup PLEASE_USE_strndup_
char	*strdup_(char const *);
char	*strndup_(char const *, size_t);

#define strlcat PLEASE_USE_strlcat_
#define strlcpy PLEASE_USE_strlcpy_
size_t	strlcat_(char *, const char *, size_t);
size_t	strlcpy_(char *, const char *, size_t);

#define strsignal PLEASE_USE_strsignal_
char	*strsignal_(int);

#define strsep PLEASE_USE_strsep_
char	*strsep_(char **, char const *);

#define STRCTV_BEGIN strctv_(
#define STRCTV_END ,strctv_sentinel_)

extern char const *strctv_sentinel_;

int	strbcmp(char const *, char const *) FUNC_NONNULL(()) FUNC_RETURNS;
char	*strctv_(char **, ...) FUNC_NONNULL((1));
int	strecmp(char const *, char const *) FUNC_NONNULL(()) FUNC_RETURNS;
char	*strtkn(char **, char const *) FUNC_RETURNS;
int32_t	strtoi32(char const *, char const **, unsigned);

#endif
