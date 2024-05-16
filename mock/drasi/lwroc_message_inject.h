/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024
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

#ifndef LWROC_MESSAGE_INJECT_H
#define LWROC_MESSAGE_INJECT_H

#include <stdarg.h>
#include <stdlib.h>

enum {
	LWROC_MSGLVL_BUG,
	LWROC_MSGLVL_DEBUG,
	LWROC_MSGLVL_ERROR,
	LWROC_MSGLVL_INFO,
	LWROC_MSGLVL_LOG,
	LWROC_MSGLVL_SPAM
};

struct timespec;

typedef char *(*lwroc_format_message_context)(char *, size_t, void const *);

void lwroc_message_internal(int, lwroc_format_message_context const *, char
    const *, int, char const *, ...);
void lwroc_message_internal_extra(int, int, struct timespec *,
    lwroc_format_message_context const *, char const *, int, char const *,
    ...);

#endif
