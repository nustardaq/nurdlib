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

#ifndef UTIL_FUNCATTR_H
#define UTIL_FUNCATTR_H

#include <nconf/util/funcattr.h>

#if NCONF_mFUNC_NONNULL_bYES
/* NCONF_NOLINK */
#	define FUNC_NONNULL(list) __attribute__((nonnull list))
#elif NCONF_mFUNC_NONNULL_bNO
/* NCONF_NOLINK */
#	define FUNC_NONNULL(list)
#endif
#if NCONFING_mFUNC_NONNULL
char *nconf_test1_(void *) FUNC_NONNULL(());
char *nconf_test2_(void *) FUNC_NONNULL((1));
#endif

#if NCONF_mFUNC_NORETURN_bYES
/* NCONF_NOLINK */
#	define FUNC_NORETURN __attribute__((noreturn))
#elif NCONF_mFUNC_NORETURN_bNO
/* NCONF_NOLINK */
#	define FUNC_NORETURN
#endif
#if NCONFING_mFUNC_NORETURN
void nconf_test(void) FUNC_NORETURN;
#endif

#if NCONF_mFUNC_PRINTF_bYES
/* NCONF_NOLINK */
#	define FUNC_PRINTF(fmt, args) \
    __attribute__((format(printf, fmt, args)))
#elif NCONF_mFUNC_PRINTF_bNO
/* NCONF_NOLINK */
#	define FUNC_PRINTF(fmt, args)
#endif
#if NCONFING_mFUNC_PRINTF
void nconf_test(int, char const *, ...) FUNC_PRINTF(2, 3);
#endif

#if NCONF_mFUNC_PURE_bYES
/* NCONF_NOLINK */
#	define FUNC_PURE __attribute__((pure))
#elif NCONF_mFUNC_PURE_bNO
/* NCONF_NOLINK */
#	define FUNC_PURE
#endif
#if NCONFING_mFUNC_PURE
void nconf_test(void) FUNC_PURE;
#endif

#if NCONF_mFUNC_RETURNS_bYES
/* NCONF_NOLINK */
#	define FUNC_RETURNS __attribute__((warn_unused_result))
#elif NCONF_mFUNC_RETURNS_bNO
/* NCONF_NOLINK */
#	define FUNC_RETURNS
#endif
#if NCONFING_mFUNC_RETURNS
int nconf_test(void) FUNC_RETURNS;
#endif

#if NCONF_mFUNC_UNUSED_bYES
/* NCONF_NOLINK */
#	define FUNC_UNUSED __attribute__((unused))
#elif NCONF_mFUNC_UNUSED_bNO
/* NCONF_NOLINK */
#	define FUNC_UNUSED
#endif
#if NCONFING_mFUNC_UNUSED
int nconf_test(void) FUNC_UNUSED;
#endif

#endif
