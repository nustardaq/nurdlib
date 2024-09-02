/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2021, 2024
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

#define _POSIX_C_SOURCE 199309L

#include <util/syscall.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <nurdlib/log.h>
#include <util/string.h>

void
system_call(char const *a_fmt, ...)
{
	va_list args;
	char cmd[1024];
	char line[256];
	FILE *fp;
	int ret;

	LOGF(verbose)(LOGL, "system_call(%s) {", a_fmt);
	va_start(args, a_fmt);
	ret = vsnprintf_(cmd, sizeof cmd, a_fmt, args);
	va_end(args);
	if (ret + 1 == sizeof cmd) {
		log_die(LOGL, "Command line too long.");
	}
	LOGF(info)(LOGL, "cmd=\"%s\"", cmd);
	fp = popen(cmd, "r");
	if (NULL == fp) {
		log_die(LOGL, "popen(%s): %s.", cmd, strerror(errno));
	}
	while (NULL != fgets(line, sizeof line, fp)) {
		LOGF(info)(LOGL, "> %s", line);
	}
	ret = pclose(fp);
	if (-1 == ret) {
		log_die(LOGL, "pclose(%s): %s.", cmd, strerror(errno));
	}
	if (0 != ret) {
		log_die(LOGL, "pclose(%s): Exit code=%d.", cmd, ret);
	}
	LOGF(verbose)(LOGL, "}");
}
