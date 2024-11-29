/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
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

#include <nurdlib/log.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <nurdlib/base.h>
#include <util/assert.h>
#include <util/fmtmod.h>
#include <util/stdint.h>
#include <util/string.h>
#include <util/time.h>

#define INDENT_MAX 10

enum SuppressState {
	SUPPRESS_STATE_DONT,
	SUPPRESS_STATE_FIRST,
	SUPPRESS_STATE_REST
};
struct LogLevel {
	unsigned	index;
};

static void	callback_stdio(char const *, int, unsigned, char const *);
static void	print(struct LogFile const *, int, enum Keyword, char const *,
    va_list, int) FUNC_PRINTF(4, 0);

#define LOG_LEVEL_DEFINE(name, index)\
    static struct LogLevel const g_##name##_ = {index};\
    struct LogLevel const *const g_log_level_##name##_ = &g_##name##_
LOG_LEVEL_DEFINE(info, 0);
LOG_LEVEL_DEFINE(verbose, 1);
LOG_LEVEL_DEFINE(debug, 2);
LOG_LEVEL_DEFINE(spam, 3);
static struct LogLevel const *g_log_stack[10] = {&g_info_};
static unsigned g_stack_i;
static unsigned g_indent;
static LogCallback g_callback = callback_stdio;
static enum SuppressState g_suppress_state;

void
callback_stdio(char const *a_file, int a_line_no, unsigned a_level, char const
    *a_str)
{
	struct tm tm;
	time_t t_now;
	char tbuf[26];
	FILE *str;
	char const *level;

	time(&t_now);
	gmtime_r_(&t_now, &tm);
	strftime(tbuf, sizeof tbuf, "%Y-%m-%d,%H:%M:%S", &tm);
	str = stdout;
	switch (a_level) {
	case KW_INFO:    level = "INFO"; break;
	case KW_VERBOSE: level = "VRBS"; break;
	case KW_DEBUG:   level = "DEBG"; break;
	case KW_SPAM:    level = "SPAM"; break;
	case KW_ERROR:   level = "ERRR"; str = stderr; break;
	default: abort();
	}
	fprintf(str, "%s:%s: %s [%s:%u]\n",
	    tbuf, level, a_str, a_file, a_line_no);
	fflush(str);
}

void
log_callback_set(LogCallback a_callback)
{
	g_callback = NULL == a_callback ? callback_stdio : a_callback;
}

void
log_die(struct LogFile const *a_file, int a_line_no, char const *a_fmt, ...)
{
	va_list args;

	va_start(args, a_fmt);
	log_errorv(a_file, a_line_no, a_fmt, args);
	va_end(args);
	log_error(a_file, a_line_no, "Calling abort()...");
	abort();
}

void
log_dump(struct LogFile const *a_file, int a_line_no, void const *a_start,
    size_t a_bytes)
{
	char line[80];
	void const *p;
	size_t i;
	size_t ofs = 0;
	int ret = -1;

	log_info_printf_(a_file, a_line_no, "---[ Dump begin ]---");
	log_info_printf_(a_file, a_line_no, "Start=%p  Bytes=%"PRIz"=0x%"PRIzx,
	    a_start, a_bytes, a_bytes);
	p = a_start;
	for (i = 0; i < a_bytes;) {
		void const *p_old;

		if (0 == (31 & i)) {
			if (0 != i) {
				log_info_printf_(a_file, a_line_no, "%s",
				    line);
			}
			ret = snprintf_(line, sizeof line, "%5"PRIzx":", i);
			if (ret < 0) {
				log_die(LOGL, "snprintf failed.");
			} else {
				ofs = ret;
			}
		}
		p_old = p;
		if (i + 3 < a_bytes) {
			uint32_t const *p32;

			p32 = p;
			ret = snprintf_(line + ofs, sizeof line - ofs, " %08x",
			    *p32);
			if (ret < 0) {
				log_die(LOGL, "snprintf failed.");
			} else {
				ofs += ret;
			}
			p = p32 + 1;
		} else if (i + 1 < a_bytes) {
			uint16_t const *p16;

			p16 = p;
			ret = snprintf_(line + ofs, sizeof line - ofs, " %04x",
			    *p16);
			if (ret < 0) {
				log_die(LOGL, "snprintf failed.");
			} else {
				ofs += ret;
			}
			p = p16 + 1;
		} else {
			uint8_t const *p8;

			p8 = p;
			ret = snprintf_(line + ofs, sizeof line - ofs, " %02x",
			    *p8);
			if (ret < 0) {
				log_die(LOGL, "snprintf failed.");
			} else {
				ofs += ret;
			}
			p = p8 + 1;
		}
		i += (uintptr_t)p - (uintptr_t)p_old;
	}
	if (ofs > 6) {
		log_info_printf_(a_file, a_line_no, "%s", line);
	}
	ASSERT(size_t, PRIz, i, ==, a_bytes);
	log_info_printf_(a_file, a_line_no, "---[  Dump end  ]---");
}

void
log_err(struct LogFile const *a_file, int a_line_no, char const *a_fmt, ...)
{
	va_list args;
	int errno_;

	errno_ = errno;
	va_start(args, a_fmt);
	print(a_file, a_line_no, KW_ERROR, a_fmt, args, errno_);
	va_end(args);
	abort();
}

void
log_verr(struct LogFile const *a_file, int a_line_no, char const *a_fmt,
    va_list a_args)
{
	print(a_file, a_line_no, KW_ERROR, a_fmt, a_args, errno);
	abort();
}

void
log_error(struct LogFile const *a_file, int a_line_no, char const *a_fmt, ...)
{
	va_list args;

	va_start(args, a_fmt);
	log_errorv(a_file, a_line_no, a_fmt, args);
	va_end(args);
}

void
log_errorv(struct LogFile const *a_file, int a_line_no, char const *a_fmt,
    va_list a_args)
{
	print(a_file, a_line_no, KW_ERROR, a_fmt, a_args, 0);
}

void
log_level_clear(void)
{
	g_stack_i = 0;
}

struct LogLevel const *
log_level_get(void)
{
	return g_log_stack[g_stack_i];
}

struct LogLevel const *
log_level_get_from_keyword(enum Keyword a_keyword)
{
	if (KW_INFO == a_keyword) {
		return g_log_level_info_;
	} else if (KW_VERBOSE == a_keyword) {
		return g_log_level_verbose_;
	} else if (KW_DEBUG == a_keyword) {
		return g_log_level_debug_;
	} else if (KW_SPAM == a_keyword) {
		return g_log_level_spam_;
	} else {
		log_die(LOGL, "Invalid log level keyword '%d'.", a_keyword);
	}
}

int
log_level_is_visible(struct LogLevel const *a_level)
{
	return a_level->index <= g_log_stack[g_stack_i]->index;
}

void
log_level_pop(void)
{
	if (0 == g_stack_i) {
		log_die(LOGL, "Tried to pop from empty log level stack.");
	}
	--g_stack_i;
}

void
log_level_push(struct LogLevel const *a_level)
{
	++g_stack_i;
	if (LENGTH(g_log_stack) == g_stack_i) {
		log_die(LOGL,
		    "Tried to push to full (%"PRIz") log level stack.",
		    LENGTH(g_log_stack));
	}
	g_log_stack[g_stack_i] = a_level;
}

#define LOG_PRINTF_DEFINE(name, level)\
void \
log_##name##_printf_(struct LogFile const *a_file, int a_line_no, char const \
    *a_fmt, ...)\
{\
	va_list args;\
\
	assert(log_level_is_visible(g_log_level_##name##_));\
	va_start(args, a_fmt);\
	switch (g_suppress_state) {\
	case SUPPRESS_STATE_DONT:\
		print(a_file, a_line_no, level, a_fmt, args, 0);\
		break;\
	case SUPPRESS_STATE_FIRST:\
		print(a_file, a_line_no, level, "Suppressed...", args, 0);\
		g_suppress_state = SUPPRESS_STATE_REST;\
		break;\
	case SUPPRESS_STATE_REST:\
		break;\
	}\
	va_end(args);\
}\
void log_##name##_printf_(struct LogFile const *, int, char const *, ...)
LOG_PRINTF_DEFINE(info, KW_INFO);
LOG_PRINTF_DEFINE(verbose, KW_VERBOSE);
LOG_PRINTF_DEFINE(debug, KW_DEBUG);
LOG_PRINTF_DEFINE(spam, KW_SPAM);

void
log_suppress_all_levels(int a_yes)
{
	if (a_yes) {
		if (SUPPRESS_STATE_DONT == g_suppress_state) {
			g_suppress_state = SUPPRESS_STATE_FIRST;
		}
	} else {
		g_suppress_state = SUPPRESS_STATE_DONT;
	}
}

void
log_warn(struct LogFile const *a_file, int a_line_no, char const *a_fmt, ...)
{
	va_list args;
	int errno_;

	errno_ = errno;
	va_start(args, a_fmt);
	print(a_file, a_line_no, KW_ERROR, a_fmt, args, errno_);
	va_end(args);
}

void
log_vwarn(struct LogFile const *a_file, int a_line_no, char const *a_fmt,
    va_list a_args)
{
	print(a_file, a_line_no, KW_ERROR, a_fmt, a_args, errno);
}

/*
 * If the last character is a '{', indent is increased.
 * If the last character is a '}', indent is decreased.
 */
void
print(struct LogFile const *a_file, int a_line_no, enum Keyword a_level, char
    const *a_fmt, va_list a_args, int a_errno)
{
	char buf[1024];
	size_t ofs;

	assert(
	    KW_INFO == a_level ||
	    KW_VERBOSE == a_level ||
	    KW_DEBUG == a_level ||
	    KW_SPAM == a_level ||
	    KW_ERROR == a_level);
	assert(g_indent < sizeof buf);
	for (ofs = 0; g_indent > ofs; ++ofs) {
		buf[ofs] = '.';
	}
	ofs += vsnprintf_(buf + ofs, sizeof buf - ofs, a_fmt, a_args);
	if (0 == a_errno) {
		char last;

		last = buf[ofs - 1];
		ofs = 0;
		if ('{' == last) {
			if (INDENT_MAX <= g_indent) {
				log_die(LOGL, "Log indent overflow: \"%s\".",
				    buf);
			}
			++g_indent;
		} else if ('}' == last) {
			if (0 == g_indent) {
				log_die(LOGL, "Log indent underflow: \"%s\".",
				    buf);
			}
			--g_indent;
			ofs = 1;
		}
	} else {
		/* Don't check indentation with err/warn suffix. */
		snprintf_(buf + ofs, sizeof buf - ofs, ": %s.",
		    strerror(errno));
		ofs = 0;
	}
	g_callback((void const *)a_file, a_line_no, a_level, buf + ofs);
}
