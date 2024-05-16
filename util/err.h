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

#ifndef UTIL_ERR_H
#define UTIL_ERR_H

#include <util/funcattr.h>
#include <stdarg.h>

typedef int (*ErrPrinter)(char const *, va_list);

void	err_set_printer(ErrPrinter);
void	err_(int, char const *, ...) FUNC_PRINTF(2, 3);
void	errx_(int, char const *, ...) FUNC_PRINTF(2, 3);
void	warn_(char const *, ...) FUNC_PRINTF(1, 2);
void	warnx_(char const *, ...) FUNC_PRINTF(1, 2);
void	vwarnx_(char const *, va_list);

#endif
