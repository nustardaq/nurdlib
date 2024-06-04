/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2024
 * Bastian Löher
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

#ifndef NURDLIB_LOG_H
#define NURDLIB_LOG_H

#include <stdarg.h>
#include <stdlib.h>
#include <nurdlib/keyword.h>
#include <util/funcattr.h>

#define LOGF(level)\
    if (log_level_is_visible(g_log_level_##level##_)) log_##level##_printf_
#define LOGL (void *)__FILE__, __LINE__
#define LOGL_ARGS struct LogFile const *, unsigned

struct LogFile;
struct LogLevel;

typedef void (*LogCallback)(char const *, unsigned, unsigned, char const *);

#define LOG_LEVEL_DECLARE_(name)\
extern struct LogLevel const *const g_log_level_##name##_;\
void	log_##name##_printf_(LOGL_ARGS, char const *, ...) FUNC_PRINTF(3, 4)

/* For quiet production running. */
LOG_LEVEL_DECLARE_(info);
/* For talkative running if you're paranoid. */
LOG_LEVEL_DECLARE_(verbose);
/* For very talkative running for developers. */
LOG_LEVEL_DECLARE_(debug);
/* For when you like to hold on tight, be prepared to abort. */
LOG_LEVEL_DECLARE_(spam);

void			log_callback_set(LogCallback);
void			log_die(LOGL_ARGS, char const *, ...)
	FUNC_PRINTF(3, 4) FUNC_NORETURN;
void			log_dump(LOGL_ARGS, void const *, size_t)
	FUNC_NONNULL(());
void			log_error(LOGL_ARGS, char const *, ...)
	FUNC_PRINTF(3, 4);
void			log_errorv(LOGL_ARGS, char const *, va_list)
	FUNC_PRINTF(3, 0);
void			log_level_clear(void);
struct LogLevel const	*log_level_get(void) FUNC_RETURNS;
struct LogLevel const	*log_level_get_from_keyword(enum Keyword)
	FUNC_RETURNS;
int			log_level_is_visible(struct LogLevel const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
void			log_level_pop(void);
void			log_level_push(struct LogLevel const *)
	FUNC_NONNULL(());
int			log_printerv(char const *, va_list) FUNC_PRINTF(1, 0)
	FUNC_RETURNS;
void			log_suppress_all_levels(int);

#endif
