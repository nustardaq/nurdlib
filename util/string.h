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

#include <nconf/util/string.h>
#include <stdarg.h>
#include <string.h>
#include <util/funcattr.h>
#include <util/stdint.h>

#if NCONF_mNPRINTF_bSTDIO
#	include <stdio.h>
#elif NCONF_mNPRINTF_bDEFAULT_SOURCE
/* NCONF_CPPFLAGS=-D_DEFAULT_SOURCE */
#	include <stdio.h>
#elif NCONF_mNPRINTF_bBSD_SOURCE
/* NCONF_CPPFLAGS=-D_BSD_SOURCE */
#	include <stdio.h>
#elif NCONF_mNPRINTF_bPROTOTYPE
int snprintf(char *, size_t, char const *, ...) FUNC_PRINTF(3, 4);
int vsnprintf(char *, size_t, char const *, va_list) FUNC_PRINTF(3, 0);
#elif NCONF_mNPRINTF_bUNSAFE
/* NCONF_SRC=util/string.c */
#	define snprintf util_snprintf_
#	define vsnprintf util_vsnprintf_
int util_snprintf_(char *, size_t, char const *, ...) FUNC_PRINTF(3, 4);
int util_vsnprintf_(char *, size_t, char const *, va_list) FUNC_PRINTF(3, 0);
#endif
#if NCONFING_mNPRINTF
#	define NCONF_TEST nprintf_test_(argc, "")
static int nprintf_test_(int i, ...) {
	va_list args;
	char s[10];
	int ret;
	va_start(args, i);
	ret = vsnprintf(s, sizeof s, "%d", args);
	va_end(args);
	return snprintf(s, sizeof s, "%d", i) + ret;
}
#endif

#if NCONF_mSTRDUP_bPOSIX_200809
/* NCONF_CPPFLAGS=-D_POSIX_C_SOURCE=200809 */
#elif NCONF_mSTRDUP_bBSD_SOURCE
/* NCONF_CPPFLAGS=-D_BSD_SOURCE */
#endif
#if NCONFING_mSTRDUP
#	define NCONF_TEST strdup(argv[0])
#endif

#if NCONF_mSTRL_bBSD_SOURCE
/* NCONF_CPPFLAGS=-D_BSD_SOURCE */
#elif NCONF_mSTRL_bDARWIN_C_SOURCE
/* NCONF_CPPFLAGS=-D_DARWIN_C_SOURCE */
#elif NCONF_mSTRL_bIMPORT
/* NCONF_SRC=util/strlcat.c util/strlcpy.c */
size_t	strlcat(char *, const char *, size_t);
size_t	strlcpy(char *, const char *, size_t);
#endif
#if NCONFING_mSTRL
#	define NCONF_TEST strl_test_(argv[0])
static int strl_test_(char const *str) {
	char s[2];
	return strlcat(s, str, sizeof s) + strlcpy(s, str, sizeof s);
}
#endif

#if NCONF_mSTRNDUP_bPOSIX_200809
/* NCONF_CPPFLAGS=-D_POSIX_C_SOURCE=200809 */
#elif NCONF_mSTRNDUP_bCUSTOM
/* NCONF_SRC=util/string.c */
#	define strndup util_strndup_
char *util_strndup_(char const *, size_t) FUNC_RETURNS;
#endif
#if NCONFING_mSTRNDUP
#	define NCONF_TEST strndup(argv[0], 1)
#endif

#if NCONF_mSTRSIGNAL_bPOSIX_200809
/* NCONF_CPPFLAGS=-D_POSIX_C_SOURCE=200809 */
#elif NCONF_mSTRSIGNAL_bPROTOTYPE
char *strsignal(int) FUNC_RETURNS;
#elif NCONF_mSTRSIGNAL_bCUSTOM
/* NCONF_SRC=util/string.c */
#	define strsignal util_strsignal_
char *util_strsignal_(int) FUNC_RETURNS;
#endif
#if NCONFING_mSTRSIGNAL
#	include <signal.h>
#	define NCONF_TEST strsignal(argc)
#endif

#define STRCTV_BEGIN strctv_(
#define STRCTV_END ,strctv_sentinel_)

extern char const *strctv_sentinel_;

int	strbcmp(char const *, char const *) FUNC_NONNULL(()) FUNC_RETURNS;
char	*strctv_(char **, ...) FUNC_NONNULL((1));
int	strecmp(char const *, char const *) FUNC_NONNULL(()) FUNC_RETURNS;
char	*strtkn(char **, char const *) FUNC_RETURNS;
int32_t	strtoi32(char const *, char const **, unsigned);

#endif
