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

#include <util/string.h>
#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <nurdlib/base.h>

int
snprintf_(char *a_dst, size_t a_dst_size, char const *a_fmt, ...)
{
	va_list args;
	int len;

	va_start(args, a_fmt);
	len = vsnprintf_(a_dst, a_dst_size, a_fmt, args);
	va_end(args);
	return len;
}

int
vsnprintf_(char *a_dst, size_t a_dst_size, char const *a_fmt, va_list a_args)
{
	int len;

	len = vsprintf(a_dst, a_fmt, a_args);
	if (a_dst_size < (size_t)len) {
		fprintf(stderr, "Overrun in util_vsnprintf_, abort!()\n");
		abort();
	}
	return len;
}

char *
strdup_(char const *a_s)
{
	char *s;
	size_t len;

	len = strlen(a_s);
	s = malloc(len + 1);
	if (NULL == s) {
		return NULL;
	}
	memmove(s, a_s, len);
	s[len] = '\0';
	return s;
}

char *
strndup_(char const *a_s, size_t a_maxlen)
{
	char *s;
	size_t len;

	len = strlen(a_s);
	len = MIN(len, a_maxlen);
	s = malloc(len + 1);
	if (NULL == s) {
		return NULL;
	}
	memmove(s, a_s, len);
	s[len] = '\0';
	return s;
}

#include <util/strlcat.h>
#include <util/strlcpy.h>

char *
strsignal_(int a_signum)
{
#	define TRANSLATE(name) case SIG##name: return #name
	switch (a_signum) {
	TRANSLATE(HUP);
	TRANSLATE(INT);
	TRANSLATE(QUIT);
	TRANSLATE(ILL);
	TRANSLATE(TRAP);
	TRANSLATE(ABRT);
	TRANSLATE(FPE);
	TRANSLATE(KILL);
	TRANSLATE(BUS);
	TRANSLATE(SEGV);
	TRANSLATE(SYS);
	TRANSLATE(PIPE);
	TRANSLATE(ALRM);
	TRANSLATE(TERM);
	TRANSLATE(URG);
	TRANSLATE(STOP);
	TRANSLATE(TSTP);
	TRANSLATE(CONT);
	TRANSLATE(CHLD);
	TRANSLATE(TTIN);
	TRANSLATE(TTOU);
	TRANSLATE(XCPU);
	TRANSLATE(XFSZ);
	TRANSLATE(VTALRM);
	TRANSLATE(PROF);
	TRANSLATE(USR1);
	TRANSLATE(USR2);
	default: return "Unknown";
	}
}

char *
strsep_(char **a_str, char const *a_delim)
{
	char *p, *ret;

	ret = *a_str;
	if (NULL == *a_str) {
		return ret;
	}
	for (p = *a_str; '\0' != *p; ++p) {
		char const *q;

		for (q = a_delim; '\0' != *q; ++q) {
			if (*p == *q) {
				*p = '\0';
				*a_str = p + 1;
				return ret;
			}
		}
	}
	*a_str = NULL;
	return ret;
}

char const *strctv_sentinel_ = (void *)&strctv_sentinel_;
static char const c_NULL[] = "NULL";

/*
 * Compares the beginning of the big string with the pattern.
 *  a_big: Big string.
 *  a_pattern: Pattern string.
 *  Returns: Similar to strcmp.
 */
int
strbcmp(char const *a_big, char const *a_pattern)
{
	char const *b;
	char const *p;

	b = a_big;
	p = a_pattern;
	for (;; ++b, ++p) {
		if ('\0' == *p) {
			return 0;
		}
		if ('\0' == *b) {
			return -1;
		}
		if (*b < *p) {
			return -1;
		}
		if (*b > *p) {
			return 1;
		}
	}
}

/*
 * Concatenates variadic strings, NULL terminated, into a malloc:d string.
 *  a_dst: Location of potentially non-empty string, may be NULL, may be
 *         realloc:ed.
 *  Returns: *a_dst on success, NULL on failure.
 */
char *
strctv_(char **a_dst, ...)
{
	va_list args;
	char const *from;
	char *dst, *ndst, *to;
	size_t dstlen, len;

	dst = *a_dst;
	len = dstlen = NULL == dst ? 0 : strlen(dst);
	va_start(args, a_dst);
	for (;;) {
		from = va_arg(args, char const *);
		if (strctv_sentinel_ == from) {
			break;
		}
		if (NULL == from) {
			len += sizeof c_NULL - 1;
		} else {
			len += strlen(from);
		}
	}
	va_end(args);
	ndst = realloc(dst, len + 1);
	if (NULL == ndst) {
		return NULL;
	}
	to = ndst + dstlen;
	va_start(args, a_dst);
	for (;;) {
		from = va_arg(args, char const *);
		if (strctv_sentinel_ == from) {
			break;
		}
		if (NULL == from) {
			memmove(to, c_NULL, sizeof c_NULL - 1);
			to += sizeof c_NULL - 1;
		} else {
			while ('\0' != *from) {
				*to++ = *from++;
			}
		}
	}
	va_end(args);
	*to = '\0';
	return *a_dst = ndst;
}

/*
 * Compares the end of the big string with the pattern.
 *  a_big: Big string.
 *  a_pattern: Pattern string.
 *  Returns: Similar to strcmp.
 */
int
strecmp(char const *a_big, char const *a_pattern)
{
	size_t big_len, pattern_len;

	big_len = strlen(a_big);
	pattern_len = strlen(a_pattern);
	if (big_len < pattern_len) {
		return -1;
	}
	return strcmp(a_big + big_len - pattern_len, a_pattern);
}

/*
 * Tokeniser, considers a group of delimiters to be a single token separator.
 * Similar to strsep, this is just a little more "comfortable".
 */
char *
strtkn(char **a_stringp, char const *a_delim)
{
	char *p, *ret;
	size_t ofs;

	p = *a_stringp;
	if (NULL == p) {
		return NULL;
	}
	ofs = strspn(p, a_delim);
	if ('\0' == p[ofs]) {
		*a_stringp = NULL;
		return NULL;
	}
	p += ofs;
	ret = p;
	p += strcspn(p, a_delim);
	if ('\0' == *p) {
		*a_stringp = NULL;
	} else {
		*p = '\0';
		*a_stringp = p + 1;
	}
	return ret;
}

/*
 * 'strtol' but expects a positive integer and does uint32 internally and then
 * casts... Check tests!
 */
int32_t
strtoi32(char const *a_s, char const **a_end, unsigned a_base)
{
	uint32_t u32, f;
	char const *p;

	assert(10 == a_base || 16 == a_base);
	for (p = a_s;; ++p) {
		if ('\0' == *p ||
		    (10 == a_base && !isdigit(*p)) ||
		    (16 == a_base && !isxdigit(*p))) {
			if (NULL != a_end) {
				*a_end = p;
			}
			break;
		}
	}
	u32 = 0;
	f = 1;
	while (p != a_s) {
		uint32_t d;

		--p;
		d = *p;
		if (d >= '0' && d <= '9') {
			d -= '0';
		} else if (d >= 'A' && d <= 'F') {
			d -= 'A' - 10;
		} else if (d >= 'a' && d <= 'f') {
			d -= 'a' - 10;
		} else {
			assert(0 && "strtoi32 logic failed!");
		}
		u32 += d * f;
		f *= a_base;
	}
	return u32;
}
