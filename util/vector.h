/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020, 2024
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

#ifndef NURDLIB_UTIL_VECTOR_H
#define NURDLIB_UTIL_VECTOR_H

/*
 * Super simple vector for appending single elements with instant array
 * resize, so should be used for low-latency or small arrays.
 */

#define VECTOR_FOREACH(it, vec) for (it = (vec)->array;\
    0 != (vec)->size && it < &(vec)->array[(vec)->size]; ++it)
#define VECTOR_FOREACH_REV(it, vec) for (it = &(vec)->array[(vec)->size - 1]; \
    0 != (vec)->size && it >= (vec)->array; --it)
#define VECTOR_FREE(vec) do {\
		(vec)->size = 0;\
		FREE((vec)->array);\
	} while (0)
#define VECTOR_HEAD(Vec, Type) struct Vec {\
	size_t	size;\
	Type	*array;\
}
#define VECTOR_INIT(vec) do {\
		(vec)->size = 0;\
		(vec)->array = NULL;\
	} while (0)
#define VECTOR_APPEND(vec, obj) do {\
		void *old = (vec)->array;\
		MALLOC((vec)->array, ((vec)->size + 1));\
		memcpy((vec)->array, old, (vec)->size * sizeof \
		    *(vec)->array);\
		free(old);\
		(vec)->array[(vec)->size++] = obj;\
	} while (0)

#endif
