/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2023-2024
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

#ifndef UTIL_ASSERT_H
#define UTIL_ASSERT_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef NDEBUG
#	define ASSERT(Type, fmt, lhs, op, rhs)
#	define STATIC_ASSERT(x)
#else
#	define ASSERT(Type, fmt, lhs, op, rhs) do { \
		Type lhs_ = lhs; \
		Type rhs_ = rhs; \
		if (!(lhs_ op rhs_)) { \
			fprintf(stderr, \
			    "%s:%d: "#lhs"=%"fmt" "#op" "#rhs"=%"fmt \
			    " failed.\n", __FILE__, __LINE__, lhs_, rhs_); \
			abort(); \
		} \
	} while (0)
#	define STATIC_ASSERT(x) do { \
		char assert_[(x) ? 1 : -1]; \
		(void)assert_; \
	} while (0)
#endif

#endif
